# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

"""Python environment for Windows a11y browser tests.
"""

import ctypes
import os
from ctypes import POINTER, byref
from ctypes.wintypes import BOOL, HWND, LPARAM, POINT  # noqa: F401

import comtypes.client
import psutil
from comtypes import COMError, IServiceProvider

user32 = ctypes.windll.user32
oleacc = ctypes.oledll.oleacc
oleaccMod = comtypes.client.GetModule("oleacc.dll")
IAccessible = oleaccMod.IAccessible
del oleaccMod
OBJID_CLIENT = -4
CHILDID_SELF = 0
NAVRELATION_EMBEDS = 0x1009
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
TreeScope_Descendants = uiaMod.TreeScope_Descendants
UIA_AutomationIdPropertyId = uiaMod.UIA_AutomationIdPropertyId
del uiaMod


def AccessibleObjectFromWindow(hwnd, objectID=OBJID_CLIENT):
    p = POINTER(IAccessible)()
    oleacc.AccessibleObjectFromWindow(
        hwnd, objectID, byref(IAccessible._iid_), byref(p)
    )
    return p


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
        name = ctypes.create_unicode_buffer(100)
        user32.GetClassNameW(hwnd, name, 100)
        if name.value != "MozillaWindowClass":
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


def getDocUia():
    """Get the IUIAutomationElement for the document being tested."""
    # We start with IAccessible2 because there's no efficient way to
    # find the document we want with UIA.
    ia2 = getDocIa2()
    return uiaClient.ElementFromIAccessible(ia2, CHILDID_SELF)


def findUiaByDomId(root, id):
    cond = uiaClient.CreatePropertyCondition(UIA_AutomationIdPropertyId, id)
    # FindFirst ignores elements in the raw tree, so we have to use
    # FindFirstBuildCache to override that, even though we don't want to cache
    # anything.
    request = uiaClient.CreateCacheRequest()
    request.TreeFilter = uiaClient.RawViewCondition
    return root.FindFirstBuildCache(TreeScope_Descendants, cond, request)
