/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

const nsIWindow = Components.interfaces.nsIWindow;
const nsITopLevelWindow = Components.interfaces.nsITopLevelWindow;
const nsIChildWindow = Components.interfaces.nsIChildWindow;

var runapprun = Components.classes["@mozilla.org/gfx/run;2"].createInstance(Components.interfaces.nsIRunAppRun)

var twin = Components.classes["@mozilla.org/gfx/window/toplevel;2"].createInstance(nsITopLevelWindow)

twin.init(null, 0, 0, 400, 400, 0)
twin.title = "window from js!"

var win = twin.QueryInterface(nsIWindow)
var cwin = Components.classes["@mozilla.org/gfx/window/child;2"].createInstance(nsIChildWindow)
cwin.init(win, 10, 10, 190, 190)

win = twin.QueryInterface(nsIWindow)
win.show()

win = cwin.QueryInterface(nsIWindow)
win.show()

runapprun.go()
