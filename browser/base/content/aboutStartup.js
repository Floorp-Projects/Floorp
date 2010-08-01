var Cc = Components.classes;
var Ci = Components.interfaces;

let launched, startup, restored;

let runtime = Cc["@mozilla.org/xre/runtime;1"].getService(Ci.nsIXULRuntime);

try {
  displayTimestamp("launched", launched = runtime.launchTimestamp);
} catch(x) { }

displayTimestamp("started", startup = runtime.startupTimestamp);
if (launched)
  displayDuration("started", startup - launched);

let ss = Cc["@mozilla.org/browser/sessionstartup;1"].getService(Ci.nsISessionStartup);
displayTimestamp("restored", restored = ss.restoredTimestamp);
displayDuration("restored", restored - startup);

function displayTimestamp(id, µs)
{
  document.getElementById(id).textContent = new Date(µs/1000) +" ("+ µs +" µs)";
}

function displayDuration(id, µs)
{
  document.getElementById(id).nextSibling.textContent = µs +" µs";
}
