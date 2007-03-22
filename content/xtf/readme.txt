XTF: An eXtensible Tag Framework
================================

XTF allows you to extend Mozilla by implementing new XML elements in
XPCOM modules. It is not a C++-version of XBL: XTF elements can be
written in any XPCOM-compatible language. I myself tend to write most
elements in JavaScript.

XTF support needs to be explicitly switched on for Mozilla builds by
specifying the configure option '--enable-xtf' (add "ac_add_options
--enabl-xtf" to your .mozconfig).

To serve a particular namespace "urn:mycompany:mynamespace" with your
own XTF elements, create an XPCOM component that implements the
interface nsIXTFElementFactory, and register it with the component
manager under the contract id
"@mozilla.org/xtf/element-factory;1?namespace=urn:mycompany:mynamespace".
Whenever Mozilla encounters a tag in your namespace it will call your
factory's 'createElement()' function.  This function should either
return a new xtf element (an object implementing 'nsIXTFElement' and
some other interfaces, depending on type and required features), or
'null'. In the later case, the implementation will build a generic XML
element.

All interfaces relevant to XTF factory modules and XTF elements can be
found in mozilla/content/xtf/public/.  The implementation code itself
is mainly spread over the directories mozilla/content/xtf/ and
mozilla/layout/xtf/.

Binding outermost elements (document elements)
============================================== 

Binding of outermost elements is not (yet) supported. Depending on
what you use xtf elements for, this might or might not be a
problem. If you use xtf to implement a whole new language, rather than
just widgets that will get used in html, xul, or svg docs, then you'll
probably want to wrap up your documents in an xml element for which
you provide some style (e.g. via a ua style sheet), so that things get
displayed instead of pretty printed. This outermost element can be in
your xtf-served namespace, as long as your xtf factory returns a
failure for this element's tagname. The implementation will then
generate a generic XML element for this element.

XTF and XUL
===========

When using XTF elements in XUL documents, note that the owner document
(both wrapper.ownerDocument & wrapper.elementNode.ownerDocument) will
be null at the time of the onCreated() call. (For XML and SVG
documents, at least wrapper.ownerDocument should be non-null.)  This
is unfortunate, since it is often advantageous to build the visual
content tree at the time of the onCreated() call and not wait until
the visual content is explicitly requested, so that attributes set on
the xtf element can be mapped directly to elements in the visual
content tree. It is possible to build the content tree using a
different document than the ultimate target document, but this in turn
leads to some subtleties with XBL-bound content - see below.


XTF and XBL
===========

XTF elements generally behave well in conjunction with XBL. There are
a few issues when using XBL-bound elements in an XTFVisual's
visualContent that arise from the fact that XBL bindings are attached
to elements at layout-time rather than content-tree contruction time:
Accessing an XBL-bound element's interfaces or JS object before
layout-time usually doesn't yield the expected result. E.g. a listbox
won't have the members 'appendItem', 'clearSelection', etc. before
layout-time, even if QI'ed for nsIDOMXULMenuListElement. Worse, if the
visual content has been constructed in a different document (because
the target doc wasn't available at the time of content tree
construction time - see above), then the JS object obtained before
layout time will be different to the one that will ultimately receive
the bound implementation, i.e. even QI'ing at a later stage will
fail. To work around these issues, XBL-bound content should a) either
be build as late as possible (i.e. when requested by the wrapper with
a call to nsIXTFVisual::GetVisualContent()) or b) if this is not a
possibility (e.g. because you would like to map attributes directly
into the visual content tree), all JS object references to the element
should be re-resolved at the time of the first layout (listen in to
DidLayout() notifications).


Bugs
====

1.
For xtf elements written in JS (and possibly C++ as well),
constructing a visual's visualContent using the same document as the
visual's leads to some nasty reference cycle which prevents the
wrapper, inner xtf element, anonymous content and possibly the whole
document from ever getting destroyed. A workaround is to construct the
visualContent in a different document (e.g. setting the document in
nsXMLContentBuilder to null, or not setting the document at all, will
lead to new content being build in a new temporary document).


2.
XBL-bound elements behave strangely if *any* XUL content underneath
them is accessed from JS too early.
Example: 

  <groupbox>
   <caption><xtf:foo/></caption>
   <label value="label text"/>
  </groupbox>

If the JS-implemented xtf element 'foo' accesses any xul content
before it receives the 'didLayout' notification, the groupbox will not
be properly build (it will not contain the label text). 'Accessing xul
content' includes any operation that leads to a js wrapper being
constructed for a xul element. E.g. if the xtf element listens in to
parentChanged-notifications, a wrapper will be build for the
notification's 'parent' parameter and groupbox construction
mysteriously fails.


3.
Some XUL interfaces can't be used via XPCOM and thus might not work as
expected. E.g. menulist's
nsIDOMXULSelectControlElement::insertItemAt() is supposed to return an
element of type nsIDOMXULSelectControlItemElement. However, since
menulist's XBL implementation of insertItemAt creates a xul element
which will only be bound when the next asynchronous layout occurs,
QI'ing to nsIDOMXULSelectControlItemElement fails. The result is that
the method call always fails when invoked through XPCOM.  A pragmatic
solution would be to change the XUL interface signatures to return
nsIDOMXULElement instead of nsIDOMXULSelectControlItemElement.


4.  
QI'ing a JS-implemented xtf element's wrapper from JS as in 

  element.QueryInterface(Components.interface.nsIXTFPrivate);

occasionally hits the assertion

  ###!!! ASSERTION: tearoff not empty in dtor: 
  '!(GetInterface()||GetNative()||GetJSObject())', 
  file /home/alex/devel/mozilla/js/src/xpconnect/src/xpcinlines.h, line 627

with the result that the QI succeeds (i.e. doesn't throw an exception)
but the interface methods/properties aren't available on the element
after the QI, even though element implements the given interface.

This seems to happen if GC kicks in *afer* the xtf element is being
queried for its interface ( resulting in the creation of an
nsXTFInterfaceAggregator) but *before* the nsXTFInterfaceAggregator
has been wrapped for JS use by XPCConvert::NativeData2JS().

The workaround is to a) either expose the interface via
getScriptingInterfaces (in which case it will be available to JS
callers automatically), or b) call QI until the interface is correctly
installed, e.g.:

  while (!element.inner)
    element.QueryInterface(Components.interface.nsIXTFPrivate);

With this code the QI should succeed in the first or second iteration.



07. October 2004 Alex Fritze <alex@croczilla.com>
