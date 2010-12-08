var data = [ -1, 0, 1, 1.5, undefined, true, false ];

// XXXbent window.atob treats |null| as the empty string, whereas worker.atob
//         and the js component loader treat it as the string 'null'. Meh.

var str = "";
for (var i = 0; i < 30; i++) {
  data.push(str);
  str += i % 2 ? "b" : "a";
}

function onmessage(event) {

  data.forEach(function(string) {
    postMessage({ type: "btoa", value: btoa(string) });
    postMessage({ type: "atob", value: atob(btoa(string)) });
  });

  var threw;
  try {
    atob();
  }
  catch(e) {
    threw = true;
  }

  if (!threw) {
    throw "atob didn't throw when called without an argument!";
  }
  threw = false;

  try {
    btoa();
  }
  catch(e) {
    threw = true;
  }

  if (!threw) {
    throw "btoa didn't throw when called without an argument!";
  }

  postMessage({ type: "done" });
}
