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
  document.getElementById(id).textContent = formatstamp(µs);
}

function displayDuration(id, µs)
{
  document.getElementById(id).nextSibling.textContent = formatµs(µs);
}

function formatstamp(µs)
{
  return new Date(µs/1000) +" ("+ formatµs(µs) +")";
}

function formatµs(µs)
{
  return µs + " µs";
}

var table = document.getElementsByTagName("table")[1];

var file = Components.classes["@mozilla.org/file/directory_service;1"]
                     .getService(Components.interfaces.nsIProperties)
                     .get("ProfD", Components.interfaces.nsIFile);
file.append("startup.sqlite");

var svc = Components.classes["@mozilla.org/storage/service;1"]
                    .getService(Components.interfaces.mozIStorageService);
var db = svc.openDatabase(file);
var query = db.createStatement("SELECT timestamp, launch, startup FROM duration");
query.executeAsync({
  handleResult: function(results)
  {
    for (let row = results.getNextRow(); row; row = results.getNextRow())
    {
        table.appendChild(tr(td(formatstamp(row.getResultByName("timestamp"))),
                             td(formatµs(l = row.getResultByName("launch"))),
                             td(formatµs(s = row.getResultByName("startup"))),
                             td(formatµs(l + s))));
    }
  },
  handleError: function(error)
  {
      table.appendChild(tr(td("Error: "+ error.message +" ("+ error.result +")")));
  },
  handleCompletion: function() { },
});

function td(str)
{
  let cell = document.createElement("td");
  cell.innerHTML = str;
  return cell;
}

function tr()
{
  let row = document.createElement("tr");
  Array.forEach(arguments, function(cell) { row.appendChild(cell); });
  return row;
}
