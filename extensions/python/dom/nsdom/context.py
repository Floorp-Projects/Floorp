# This is vaguely related to an nsIScriptContext.  The pydom C code
# just calls back into this class - but that C code still retains some of the
# functionality.

import sys, types, new, logging

import domcompile

from xpcom.client import Component
from xpcom import components, primitives, COMException, nsError, _xpcom

import _nsdom

GlobalWindowIIDs = [_nsdom.IID_nsIScriptGlobalObject,
                    components.interfaces.nsIDOMWindow]

IID_nsIDOMXULElement = components.interfaces.nsIDOMXULElement

# Borrow the xpcom logger (but use a new sub-logger)
logger = logging.getLogger("xpcom.nsdom")
# Note that many calls to logger.debug are prefixed with if __debug__.
# This is to prevent *any* penalty in non-debug mode for logging in perf
# critical areas.

# Also note that assert statements have no cost in non-debug builds.

# XUL Elements don't expose the XBL implemented interfaces via classinfo.
# So we cheat :)
# Most support nsIAccessibleProvider - don't list that!
# Although this sucks for now, if you strike something that isn't listed,
# then simply QI the object you have for the interface you need to use. To
# avoid the explicit QI, you could also have your code do:
#    import nsdom.context
#    xei = nsdom.context.xul_element_interfaces
#    # flag for manual inspection should a future version include it
#    if 'somewidget' in xei: raise RuntimeError, "already here!?"
#    xei['somewidget'] = [...]

xul_element_interfaces = {
    # tagName:      [IID, ...]
    'textbox':      [components.interfaces.nsIDOMXULTextBoxElement],
    'button':       [components.interfaces.nsIDOMXULButtonElement],
    'checkbox':     [components.interfaces.nsIDOMXULCheckboxElement],
    'image':        [components.interfaces.nsIDOMXULImageElement],
    'tree':         [components.interfaces.nsIDOMXULTreeElement,
                     components.interfaces.nsIDOMXULMultiSelectControlElement],
    'listbox':      [components.interfaces.nsIDOMXULMultiSelectControlElement],
    'menu':         [components.interfaces.nsIDOMXULSelectControlItemElement],
    'popup':        [components.interfaces.nsIDOMXULPopupElement],
    'radiogroup':   [components.interfaces.nsIDOMXULSelectControlElement],
}

def dump(fmt, *args):
    """A global 'dump' function, available in global namespaces.  Similar to
    The JS version, but this supports % message formatting.

    Main advantage over 'print' is that it catches the possible IO error
    when there is no available console.
    """
    try:
        sys.stderr.write(fmt % args)
        sys.stderr.write("\n")
    except IOError:
        pass

# The event listener class we attach to an object for addEventListener
class EventListener:
    _com_interfaces_ = components.interfaces.nsIDOMEventListener
    def __init__(self, context, evt_name, handler, globs = None):
        self.context = context
        if callable(handler):
            self.func = handler
        else:
            # event name for addEventListener has no 'on' prefix
            func_name = "on" + evt_name
            # What to do about arg names?  It looks like onerror
            # still only gets one arg here anyway - handleEvent only takes
            # one!
            arg_names = ('event',)
            co = domcompile.compile_function(handler,
                                             "inline event '%s'" % evt_name,
                                             func_name,
                                             arg_names)
            # Could use globs here, but should not be necessary - all we
            # are doing is getting the function object from it.
            g = {}
            exec co in g
            self.func = g[func_name]
        self.globals = globs

    def handleEvent(self, event):
        # Although handler is already a function object, we must re-bind to
        # new globals
        if self.globals is not None:
            f = new.function(self.func.func_code, self.globals, self.func.func_name)
        else:
            f = self.func
        # Convert the raw pyxpcom object to a magic _nsdom one, that
        # knows how to remember expandos etc based on context (ie, winds
        # up calling back into MakeInterfaceResult().
        event = _nsdom.MakeDOMObject(self.context, event)
        
        args = (event,)
        # We support having less args declared than supplied, a-la JS.
        # (This can only happen when passed a function object - we always
        # compile a string handler into a function with 1 arg)
        args = args[:f.func_code.co_argcount]
        return f(*args)

class WrappedNative(Component):
    """Implements the xpconnect concept of 'wrapped natives' and 'expandos'.

    DOM objects can have arbitrary values set on them.  Once this is done for
    the first time, it gets stored in a map in the context.  This leads to
    cycles, which must be cleaned up when the context is closed.
    """
    def __init__(self, context, obj, iid):
        # Store our context - but our context doesn't keep a reference
        # to us until someone calls self._remember_object() on the context -
        # which sets up all kinds of cycles!
        self.__dict__['_context_'] = context
        # We store expandos in a seperate dict rather than directly in our
        # __dict__.  No real need for this other than to prevent these
        # attributes clobbering ones we need to work!
        self.__dict__['_expandos_'] = {}
        Component.__init__(self, obj, iid)

    def __repr__(self):
        iface_desc = self._get_classinfo_repr_()
        return "<DOM object '%s' (%s)>" % (self._object_name_,iface_desc)
        
    def __getattr__(self, attr):
        # If it exists in expandos, always return it.
        if attr.startswith("__"):
            raise AttributeError, attr
        # expandos come before interface attributes (which may be wrong.  If
        # we do this, why not just store it in __dict__?)
        expandos = self._expandos_
        if expandos.has_key(attr):
            return expandos[attr]
        return Component.__getattr__(self, attr)

    def __setattr__(self, attr, value):
        try:
            Component.__setattr__(self, attr, value)
        except AttributeError:
            # Set it as an 'expando'.  It looks like we should delegate *all*
            # to the outside object.
            logger.debug("%s set expando property %r=%r for object %r",
                         self, attr, value, self._comobj_)
            # and register if an event.
            if attr.startswith("on"):
                # I'm quite confused by this :(
                target = self._comobj_
                if _nsdom.IsOuterWindow(target):
                    target = _nsdom.GetCurrentInnerWindow(target)
                go = self._context_.globalObject
                scope = self._context_.GetNativeGlobal()
                if callable(value):
                    # no idea if this is right - set the compiled object ourselves.
                    self._expandos_[attr] = value
                    _nsdom.RegisterScriptEventListener(go, scope, target, attr)
                else:
                    _nsdom.AddScriptEventListener(target, attr, value, False, False)
                    _nsdom.CompileScriptEventListener(go, scope, target, attr)
            else:
                self._expandos_[attr] = value
            self._context_._remember_object(self)

    def _build_all_supported_interfaces_(self):
        # Generally called as pyxpcom is finding an attribute, overridden to
        # work around lack of class info for xbl bindings.
        Component._build_all_supported_interfaces_(self)
        # Now hard-code certain element names we know about, as the XBL
        # implemented interfaces are not exposed by this.
        ii = self.__dict__['_interface_infos_']
        if ii.has_key(IID_nsIDOMXULElement):
            # Is a DOM element - see if in our map.
            tagName = self.tagName
            interfaces = xul_element_interfaces.get(tagName, [])
            for interface in interfaces:
                if not ii.has_key(interface):
                        self._remember_interface_info(interface)
        else:
            logger.info("Not a DOM element - not hooking extra interfaces")

    def addEventListener(self, event, handler, useCapture=False):
        # We need to transform string or function objects into
        # nsIDOMEventListener interfaces.
        logger.debug("addEventListener for %r, event=%r, handler=%r, cap=%s",
                     self, event, handler, useCapture)

        if not hasattr(handler, "handleEvent"): # may already be a handler instance.
            # Wrap it in our instance, which knows how to convert strings and
            # function objects.
            # Unlike JS, we always want to execute in the context of our
            # "window" (which the user sees very much like a "module")
            ns = self._context_.globalNamespace
            ns = ns.get("_inner_", ns) # If available, use inner!
            handler = EventListener(self._context_, event, handler, ns)

        base = self.__getattr__('addEventListener')
        base(event, handler, useCapture)

class WrappedNativeGlobal(WrappedNative):
    # Special support for our global object.  Certain methods exposed by
    # IDL are JS specific - generally ones that take a variable number of args,
    # a concept that doesn't exist in xpcom.
    def __repr__(self):
        iface_desc = self._get_classinfo_repr_()
        outer = _nsdom.IsOuterWindow(self._comobj_)
        return "<GlobalWindow outer=%s %s>" % (outer, iface_desc)

    # Open window/dialog
    def openDialog(self, url, name, features="", *args):
        svc = components.classes['@mozilla.org/embedcomp/window-watcher;1'] \
                .getService(components.interfaces.nsIWindowWatcher)
        # Wrap in an nsIArray with special support for Python being able to
        # access the raw objects if the call-site is smart enough
        args = _nsdom.MakeArray(args)
        ret = svc.openWindow(self._comobj_, url, name, features, args)
        # and re-wrap in one of our "dom" objects
        return _nsdom.MakeDOMObject(self._context_, ret)

    # window.open and window.openDialog seem identical except for *args???
    open = openDialog

    # Timeout related functions.
    def setTimeout(self, interval, handler, *args):
        return _nsdom.SetTimeoutOrInterval(self._comobj_, interval, handler,
                                           args, False)

    # setInterval appears to have reversed args???
    def setInterval(self, handler, interval, *args):
        return _nsdom.SetTimeoutOrInterval(self._comobj_, interval, handler,
                                           args, True)

    def clearTimeout(self, tid):
        return _nsdom.ClearTimeoutOrInterval(self._comobj_, tid)

    def clearInterval(self, tid):
        return _nsdom.ClearTimeoutOrInterval(self._comobj_, tid)

# Our "script context" - morally an nsIScriptContext, although that lives
# in our C++ code and delegates to this.
class ScriptContext:
    def __init__(self):
        self.globalNamespace = {} # must not change identity!
        self._remembered_objects_ = {} # could, but doesn't
        self._reset()

    def _reset(self):
        # Explicitly wipe all 'expandos' for our remembered objects.
        # Its not clear this is necessary, but it is easy to envisage someone
        # setting up a cycle via expandos.
        for ro in self._remembered_objects_.itervalues():
            ro._expandos_.clear()
        self._remembered_objects_.clear()
        # self.globalObject is the "outer window" global, whereas the
        # inner window global tends to be passed in to each function as the
        # 'scope' object.
        self.globalObject = None
        self.globalNamespace.clear()

    def __repr__(self):
        return "<ScriptContext at %d>" % id(self)

    # Called by the _nsdom C++ support to wrap the DOM objects.
    def MakeInterfaceResult(self, object, iid):
        if 0 and __debug__: # this is a little noisy...
            logger.debug("MakeInterfaceResult for %r (remembered=%s)",
                         object,
                         self._remembered_objects_.has_key(object))
        assert not hasattr(object, "_comobj_"), "should not be wrapped!"
        # If it is remembered, just return that object.
        try:
            return self._remembered_objects_[object]
        except KeyError:
            # Make a new wrapper - but don't remember the wrapper until
            # we need to, when a property is set on it.

            # We should probably QI for nsIClassInfo, and only do this special
            # wrapping for objects with the DOM flag set.
        
            if iid in GlobalWindowIIDs:
                klass = WrappedNativeGlobal
            else:
                klass = WrappedNative
            return klass(self, object, iid)

    # Called whenever this object must be "permanently" attached to the
    # context.  Typically this means an attribute or event handler
    # has been set on the object, which should "persist" while the context
    # is alive (ie, future requests to getElementById(), for example, must
    # return the same underlying object, with event handlers and properties
    # still in-place.
    def _remember_object(self, object):
        # You must only try and remember a wrapped object.
        # Once an object has been wrapped once, all further requests must
        # be identical
        assert self._remembered_objects_.get(object._comobj_, object)==object, \
               "Previously remembered object is not this object!"
        self._remembered_objects_[object._comobj_] = object
        if __debug__:
            logger.debug("%s remembering object %r - now %d items", self,
                         object, len(self._remembered_objects_))

    def _fixArg(self, arg):
        if arg is None:
            return []
        # Already a tuple?  This means the original Python args have been
        # found and passed directly.
        if type(arg) == types.TupleType:
            return arg
        try:
            argv = arg.QueryInterface(components.interfaces.nsIArray)
        except COMException, why:
            if why.errno != nsError.NS_NOINTERFACE:
                raise
            # This is not an array - see if it is a variant or primitive.
            try:
                var = arg.queryInterface(components.interfaces.nsIVariant)
                parent = None
                if self.globalObject is not None:
                    parent = self.globalObject._comobj_
                if parent is None:
                    logger.warning("_fixArg for context with no global??")
                return _xpcom.GetVariantValue(var, parent)
            except COMException, why:
                if why.errno != nsError.NS_NOINTERFACE:
                    raise
                try:
                    return primitives.GetPrimitive(arg)
                except COMException, why:
                    if why.errno != nsError.NS_NOINTERFACE:
                        raise
                return arg
        # Its an array - do each item
        ret = []
        for i in range(argv.length):
            val = argv.queryElementAt(i, components.interfaces.nsISupports)
            ret.append(self._fixArg(val))
        return ret

    def GetNativeGlobal(self):
        return self.globalNamespace

    def CreateNativeGlobalForInner(self, globalObject, isChrome):
        ret = dict()
        if __debug__:
            logger.debug("%r CreateNativeGlobalForInner returning %d",
                         self, id(ret))
        return ret

    def ConnectToInner(self, newInner, globalScope, innerScope):
        if __debug__:
            logger.debug("%r ConnectToInner(%r, %d, %d)",
                         self, newInner, id(globalScope), id(innerScope))
        globalScope['_inner_'] = innerScope # will do for now.

        self._prepareGlobalNamespace(newInner, innerScope)

    def WillInitializeContext(self):
        if __debug__:
            logger.debug("%r WillInitializeContent", self)

    def DidInitializeContext(self):
        if __debug__:
            logger.debug("%r DidInitializeContent", self)

    def DidSetDocument(self, doc, scope):
        if __debug__:
            logger.debug("%r DidSetDocument doc=%r scope=%d",
                         self, doc, id(scope))
        scope['document'] = doc

    def _prepareGlobalNamespace(self, globalObject, globalNamespace):
        assert isinstance(globalObject, WrappedNativeGlobal), \
               "Our global should have been wrapped in WrappedNativeGlobal"
        if __debug__:
            logger.debug("%r initializing (outer=%s), ns=%d", self,
                         _nsdom.IsOuterWindow(globalObject),
                         id(globalNamespace))
        assert len(globalObject._expandos_)==0, \
               "already initialized this global?"
        ns = globalObject.__dict__['_expandos_'] = globalNamespace
        self._remember_object(globalObject)
        ns['window'] = globalObject
        # we can't fetch 'document' and stash it now - there may not be one
        # at this instant (ie, it may be in the process of being attached)
        # Poke some other utilities etc into the namespace.
        ns['dump'] = dump

    def InitContext(self, globalObject):
        self._reset()
        self.globalObject = globalObject
        if globalObject is None:
            logger.debug("%r initializing with NULL global, ns=%d", self,
                         id(self.globalNamespace))
        else:
            self._prepareGlobalNamespace(globalObject, self.globalNamespace)

    def FinalizeClasses(self, globalObject):
        self._reset()

    def ClearScope(self, globalObject):
        if __debug__:
            logger.debug("%s.ClearScope (%d)", self, id(globalObject))
        globalObject.clear()

    def FinalizeContext(self):
        if __debug__:
            logger.debug("%s.FinalizeContext", self)
        self._reset()

    def EvaluateString(self, script, glob, principal, url, lineno, ver):
        # This appears misnamed - return value it not used.  Therefore we can
        # just do a simple 'exec'.  If we needed a return value, we would have
        # to treat this like a string event-handler, and compile as a temp
        # function.
        assert type(glob) == types.DictType
        # compile then exec to make use of lineno
        co = domcompile.compile(script, url, lineno=lineno-1)
        exec co in glob

    def ExecuteScript(self, scriptObject, scopeObject):
        if __debug__:
            logger.debug("%s.ExecuteScript %r in scope %s",
                         self, scriptObject, id(scopeObject))
        if scopeObject is None:
            scopeObject = self.GetNativeGlobal()
        assert type(scriptObject) == types.CodeType, \
               "Script object should be a code object (got %r)" % (scriptObject,)
        exec scriptObject in scopeObject

    def CompileScript(self, text, scopeObject, principal, url, lineno, version):
        # The line number passed is the first; offset is -1
        return domcompile.compile(text, url, lineno=lineno-1)

    def CompileEventHandler(self, name, argNames, body, url, lineno):
        co = domcompile.compile_function(body, url, name, argNames,
                                         lineno=lineno-1)
        g = {}
        exec co in g
        return g[name]

    def CallEventHandler(self, target, scope, handler, argv):
        if __debug__:
            logger.debug("CallEventHandler %r on target %s in scope %s",
                         handler, target, id(scope))
        # Event handlers must fire in our real globals (not a copy) so
        # it can set global vars!
        globs = scope
        # Although handler is already a function object, we must re-bind to
        # new globals
        f = new.function(handler.func_code, globs, handler.func_name,
                         handler.func_defaults)
        args = tuple(self._fixArg(argv))
        # We support having less args declared than supplied, a-la JS.
        args = args[:handler.func_code.co_argcount]
        return f(*args)

    def BindCompiledEventHandler(self, target, scope, name, handler):
        if __debug__:
            logger.debug("%s.BindCompiledEventHandler (%s=%r) on target %s in scope %s",
                         self, name, handler, target, id(scope))
        # Keeps a ref to both the target and handler.
        self._remember_object(target)
        ns = target._expandos_
        ns[name] = handler

    def GetBoundEventHandler(self, target, scope, name):
        if __debug__:
            logger.debug("%s.GetBoundEventHandler for '%s' on target %s in scope %s",
                         self, name, target, id(scope))
        ns = target._expandos_
        return ns[name]

    def SetProperty(self, target, name, value):
        if __debug__:
            logger.debug("%s.SetProperty for %s=%r", self, name, value)
        target[name] = self._fixArg(value)
