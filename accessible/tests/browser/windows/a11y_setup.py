# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Python environment for Windows a11y browser tests.
"""

import ctypes
import os
from ctypes import POINTER, byref
from ctypes.wintypes import BOOL, HWND, LPARAM, POINT  # noqa: F401
from dataclasses import dataclass

import comtypes.automation
import comtypes.client
import psutil
from comtypes import COMError, IServiceProvider

CHILDID_SELF = 0
COWAIT_DEFAULT = 0
EVENT_OBJECT_FOCUS = 0x8005
GA_ROOT = 2
NAVRELATION_EMBEDS = 0x1009
OBJID_CLIENT = -4
RPC_S_CALLPENDING = -2147417835
WINEVENT_OUTOFCONTEXT = 0
WM_CLOSE = 0x0010

user32 = ctypes.windll.user32
oleacc = ctypes.oledll.oleacc
oleaccMod = comtypes.client.GetModule("oleacc.dll")
IAccessible = oleaccMod.IAccessible
del oleaccMod
# This is the path if running locally.
ia2Tlb = os.path.join(
    os.getcwd(),
    "..",
    "..",
    "..",
    "accessible",
    "interfaces",
    "ia2",
    "IA2Typelib.tlb",
)
if not os.path.isfile(ia2Tlb):
    # This is the path if running in CI.
    ia2Tlb = os.path.join(os.getcwd(), "ia2Typelib.tlb")
ia2Mod = comtypes.client.GetModule(ia2Tlb)
del ia2Tlb
# Shove all the IAccessible* interfaces and IA2_* constants directly
# into our namespace for convenience.
globals().update((k, getattr(ia2Mod, k)) for k in ia2Mod.__all__)
# We use this below. The linter doesn't understand our globals() update hack.
IAccessible2 = ia2Mod.IAccessible2
del ia2Mod

uiaMod = comtypes.client.GetModule("UIAutomationCore.dll")
globals().update((k, getattr(uiaMod, k)) for k in uiaMod.__all__)
uiaClient = comtypes.CoCreateInstance(
    uiaMod.CUIAutomation._reg_clsid_,
    interface=uiaMod.IUIAutomation,
    clsctx=comtypes.CLSCTX_INPROC_SERVER,
)


def AccessibleObjectFromWindow(hwnd, objectID=OBJID_CLIENT):
    p = POINTER(IAccessible)()
    oleacc.AccessibleObjectFromWindow(
        hwnd, objectID, byref(IAccessible._iid_), byref(p)
    )
    return p


def getWindowClass(hwnd):
    MAX_CHARS = 257
    buffer = ctypes.create_unicode_buffer(MAX_CHARS)
    user32.GetClassNameW(hwnd, buffer, MAX_CHARS)
    return buffer.value


def getFirefoxHwnd():
    """Search all top level windows for the Firefox instance being
    tested.
    We search by window class name and window title prefix.
    """
    # We can compare the grandparent process ids to find the Firefox started by
    # the test harness.
    commonPid = psutil.Process().parent().ppid()
    # We need something mutable to store the result from the callback.
    found = []

    @ctypes.WINFUNCTYPE(BOOL, HWND, LPARAM)
    def callback(hwnd, lParam):
        if getWindowClass(hwnd) != "MozillaWindowClass":
            return True
        pid = ctypes.wintypes.DWORD()
        user32.GetWindowThreadProcessId(hwnd, byref(pid))
        if psutil.Process(pid.value).parent().ppid() != commonPid:
            return True  # Not the Firefox being tested.
        found.append(hwnd)
        return False

    user32.EnumWindows(callback, LPARAM(0))
    if not found:
        raise LookupError("Couldn't find Firefox HWND")
    return found[0]


def toIa2(obj):
    serv = obj.QueryInterface(IServiceProvider)
    return serv.QueryService(IAccessible2._iid_, IAccessible2)


def getDocIa2():
    """Get the IAccessible2 for the document being tested."""
    hwnd = getFirefoxHwnd()
    root = AccessibleObjectFromWindow(hwnd)
    doc = root.accNavigate(NAVRELATION_EMBEDS, 0)
    try:
        child = toIa2(doc.accChild(1))
        if "id:default-iframe-id;" in child.attributes:
            # This is an iframe or remoteIframe test.
            doc = child.accChild(1)
    except COMError:
        pass  # No child.
    return toIa2(doc)


def findIa2ByDomId(root, id):
    search = f"id:{id};"
    # Child ids begin at 1.
    for i in range(1, root.accChildCount + 1):
        child = toIa2(root.accChild(i))
        if search in child.attributes:
            return child
        descendant = findIa2ByDomId(child, id)
        if descendant:
            return descendant


@dataclass
class WinEvent:
    event: int
    hwnd: int
    objectId: int
    childId: int

    def getIa2(self):
        acc = ctypes.POINTER(IAccessible)()
        child = comtypes.automation.VARIANT()
        ctypes.oledll.oleacc.AccessibleObjectFromEvent(
            self.hwnd,
            self.objectId,
            self.childId,
            ctypes.byref(acc),
            ctypes.byref(child),
        )
        if child.value != CHILDID_SELF:
            # This isn't an IAccessible2 object.
            return None
        return toIa2(acc)


class WaitForWinEvent:
    """Wait for a win event, usually for IAccessible2.
    This should be used as follows:
    1. Create an instance to wait for the desired event.
    2. Perform the action that should fire the event.
    3. Call wait() on the instance you created in 1) to wait for the event.
    """

    def __init__(self, eventId, match):
        """eventId is the event id to wait for.
        match is either None to match any object, an str containing the DOM id
        of the desired object, or a function taking a WinEvent which should
        return True if this is the requested event.
        """
        self._matched = None
        # A kernel event used to signal when we get the desired event.
        self._signal = ctypes.windll.kernel32.CreateEventW(None, True, False, None)

        # We define this as a nested function because it has to be a static
        # function, but we need a reference to self.
        @ctypes.WINFUNCTYPE(
            None,
            ctypes.wintypes.HANDLE,
            ctypes.wintypes.DWORD,
            ctypes.wintypes.HWND,
            ctypes.wintypes.LONG,
            ctypes.wintypes.LONG,
            ctypes.wintypes.DWORD,
            ctypes.wintypes.DWORD,
        )
        def winEventProc(hook, eventId, hwnd, objectId, childId, thread, time):
            event = WinEvent(eventId, hwnd, objectId, childId)
            if isinstance(match, str):
                try:
                    ia2 = event.getIa2()
                    if f"id:{match};" in ia2.attributes:
                        self._matched = event
                except (comtypes.COMError, TypeError):
                    pass
            elif callable(match):
                try:
                    if match(event):
                        self._matched = event
                except Exception as e:
                    self._matched = e
            if self._matched:
                ctypes.windll.kernel32.SetEvent(self._signal)

        self._hook = user32.SetWinEventHook(
            eventId, eventId, None, winEventProc, 0, 0, WINEVENT_OUTOFCONTEXT
        )
        # Hold a reference to winEventProc so it doesn't get destroyed.
        self._proc = winEventProc

    def wait(self):
        """Wait for and return the desired WinEvent."""
        # Pump Windows messages until we get the desired event, which will be
        # signalled using a kernel event.
        handles = (ctypes.c_void_p * 1)(self._signal)
        index = ctypes.wintypes.DWORD()
        TIMEOUT = 10000
        try:
            ctypes.oledll.ole32.CoWaitForMultipleHandles(
                COWAIT_DEFAULT, TIMEOUT, 1, handles, ctypes.byref(index)
            )
        except WindowsError as e:
            if e.winerror == RPC_S_CALLPENDING:
                raise TimeoutError("Timeout before desired event received")
            raise
        finally:
            user32.UnhookWinEvent(self._hook)
            ctypes.windll.kernel32.CloseHandle(self._signal)
            self._proc = None
        if isinstance(self._matched, Exception):
            raise self._matched from self._matched
        return self._matched


def getDocUia():
    """Get the IUIAutomationElement for the document being tested."""
    # There's no efficient way to find the document we want with UIA. We can't
    # get the IA2 and then get UIA from that because that will always use the
    # IA2 -> UIA proxy, but we don't want that if we're trying to test our
    # native implementation. For now, we just search the tree. In future, we
    # could perhaps implement a custom property.
    hwnd = getFirefoxHwnd()
    root = uiaClient.ElementFromHandle(hwnd)
    doc = findUiaByDomId(root, "body")
    if not doc:
        # Sometimes, when UIA is disabled, we can't find the document for some
        # unknown reason. Since this only happens when UIA is disabled, we want
        # the IA2 -> UIA proxy anyway, so we can start with IA2 in this case.
        info("getUiaDoc: Falling back to IA2")  # noqa: F821
        ia2 = getDocIa2()
        return uiaClient.ElementFromIAccessible(ia2, CHILDID_SELF)
    child = uiaClient.RawViewWalker.GetFirstChildElement(doc)
    if child and child.CurrentAutomationId == "default-iframe-id":
        # This is an iframe or remoteIframe test.
        doc = uiaClient.RawViewWalker.GetFirstChildElement(child)
    return doc


def findUiaByDomId(root, id):
    cond = uiaClient.CreatePropertyCondition(uiaMod.UIA_AutomationIdPropertyId, id)
    # FindFirst ignores elements in the raw tree, so we have to use
    # FindFirstBuildCache to override that, even though we don't want to cache
    # anything.
    request = uiaClient.CreateCacheRequest()
    request.TreeFilter = uiaClient.RawViewCondition
    return root.FindFirstBuildCache(uiaMod.TreeScope_Descendants, cond, request)


class WaitForUiaEvent(comtypes.COMObject):
    """Wait for a UIA event.
    This should be used as follows:
    1. Create an instance to wait for the desired event.
    2. Perform the action that should fire the event.
    3. Call wait() on the instance you created in 1) to wait for the event.
    """

    # This tells comtypes which COM interfaces we implement. It will then call
    # either `ISomeInterface_SomeMethod` or just `SomeMethod` on this instance
    # when that method is called using COM. We use the shorter convention, since
    # we don't anticipate method name conflicts with UIA interfaces.
    _com_interfaces_ = [
        uiaMod.IUIAutomationFocusChangedEventHandler,
        uiaMod.IUIAutomationPropertyChangedEventHandler,
    ]

    def __init__(self, *, eventId=None, property=None, match=None):
        """eventId is the event id to wait for. Alternatively, you can pass
        property to wait for a particular property to change.
        match is either None to match any object, an str containing the DOM id
        of the desired object, or a function taking a IUIAutomationElement which
        should return True if this is the requested event.
        """
        self._match = match
        self._matched = None
        # A kernel event used to signal when we get the desired event.
        self._signal = ctypes.windll.kernel32.CreateEventW(None, True, False, None)
        if eventId == uiaMod.UIA_AutomationFocusChangedEventId:
            uiaClient.AddFocusChangedEventHandler(None, self)
        elif property:
            uiaClient.AddPropertyChangedEventHandler(
                uiaClient.GetRootElement(),
                uiaMod.TreeScope_Subtree,
                None,
                self,
                [property],
            )
        else:
            raise ValueError("No supported event specified")

    def _checkMatch(self, sender):
        if isinstance(self._match, str):
            try:
                if sender.CurrentAutomationId == self._match:
                    self._matched = sender
            except comtypes.COMError:
                pass
        elif callable(self._match):
            try:
                if self._match(sender):
                    self._matched = sender
            except Exception as e:
                self._matched = e
        else:
            self._matched = sender
        if self._matched:
            ctypes.windll.kernel32.SetEvent(self._signal)

    def HandleFocusChangedEvent(self, sender):
        self._checkMatch(sender)

    def HandlePropertyChangedEvent(self, sender, propertyId, newValue):
        self._checkMatch(sender)

    def wait(self):
        """Wait for and return the IUIAutomationElement which sent the desired
        event."""
        # Pump Windows messages until we get the desired event, which will be
        # signalled using a kernel event.
        handles = (ctypes.c_void_p * 1)(self._signal)
        index = ctypes.wintypes.DWORD()
        TIMEOUT = 10000
        try:
            ctypes.oledll.ole32.CoWaitForMultipleHandles(
                COWAIT_DEFAULT, TIMEOUT, 1, handles, ctypes.byref(index)
            )
        except WindowsError as e:
            if e.winerror == RPC_S_CALLPENDING:
                raise TimeoutError("Timeout before desired event received")
            raise
        finally:
            uiaClient.RemoveAllEventHandlers()
            ctypes.windll.kernel32.CloseHandle(self._signal)
        if isinstance(self._matched, Exception):
            raise self._matched from self._matched
        return self._matched
