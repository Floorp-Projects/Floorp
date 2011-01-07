netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
const Cc = Components.classes;
const Ci = Components.interfaces;
const RETURN_CONTINUE = Ci.jsdIExecutionHook.RETURN_CONTINUE;
const DebuggerService = Cc["@mozilla.org/js/jsd/debugger-service;1"];

var jsd = Components.classes['@mozilla.org/js/jsd/debugger-service;1']
                    .getService(Ci.jsdIDebuggerService);
var jsdOnAtStart = false;

function setupJSD(test) {
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  jsdOnAtStart = jsd.isOn;
  if (jsdOnAtStart) {
      runTest();
  } else {
      jsd.asyncOn({ onDebuggerActivated: function() { runTest(); } });
  }
}

// Ugly workaround: when you turn the debugger on, it will only see scripts
// compiled after that point. And it may be turned on asynchronously. So
// we put the debugged code into a separate script that gets loaded after
// the debugger is on.
function loadScript(url, element) {
    var script = document.createElement('script');
    script.type = 'text/javascript';
    script.src = url;
    script.defer = false;
    element.appendChild(script);
}

function findScriptByFunction(name) {
    var script;
    jsd.enumerateScripts({ enumerateScript:
                           function(script_) {
                               if (script_.functionName === name) {
                                   script = script_;
                               }
                           }
                         });

    if (typeof(script) === "undefined") {
        throw("Cannot find function named '" + name + "'");
    }

    return script;
}

// Pass in a JSD script
function breakOnAllLines(script) {
    // Map each line to a PC, and collect that set of PCs (removing
    // duplicates.)
    var pcs = {};
    for (i = 0; i < script.lineExtent; i++) {
        var jsdLine = script.baseLineNumber + i;
        var pc = script.lineToPc(jsdLine, Ci.jsdIScript.PCMAP_SOURCETEXT);
        pcs[pc] = 1;
    }
        
    // Set a breakpoint on each of those PCs.
    for (pc in pcs) {
        try {
            script.setBreakpoint(pc);
        } catch(e) {
            alert("Error setting breakpoint: " + e);
        }
    }
}

// Set a breakpoint on a script, where lineno is relative to the beginning
// of the script (NOT the absolute line number within the file).
function breakOnLine(script, lineno) {
    breakOnAbsoluteLine(script, script.baseLineNumber + lineno);
}

function breakOnAbsoluteLine(script, lineno) {
    var pc = script.lineToPc(lineno, Ci.jsdIScript.PCMAP_SOURCETEXT);
    script.setBreakpoint(pc);
}

function loadPage(page) {
    var url;
    if (page.match(/^\w+:/)) {
        // Full URI, so just use it
        url = page;
    } else {
        // Treat as relative to previous page
        url = document.location.href.replace(/\/[^\/]*$/, "/" + page);
    }

    dump("Switching to URL " + url + "\n");

    gURLBar.value = url;
    gURLBar.handleCommand();
}

function breakpointObserver(lines, interesting, callback) {
    jsd.breakpointHook = { onExecute: function(frame, type, rv) {
        breakpoints_hit.push(frame.line);
        if (frame.line in interesting) {
            return callback(frame, type, breakpoints_hit);
        } else {
            return RETURN_CONTINUE;
        }
    } };
}

function dumpStack(frame, msg) {
    netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
    dump(msg + ":\n");
    while(frame) {
        var callee = frame.callee;
        if (callee !== null)
          callee = callee.jsClassName;
        dump("  " + frame.script.fileName + ":" + frame.line + " func=" + frame.script.functionName + " ffunc=" + frame.functionName + " callee=" + callee + " pc=" + frame.pc + "\n");
        frame = frame.callingFrame;
    }
}
