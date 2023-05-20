const CC = Components.Constructor;

const BinaryInputStream = CC(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);
const BinaryOutputStream = CC(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);
const Timer = CC("@mozilla.org/timer;1", "nsITimer", "initWithCallback");

function handleRequest(request, response) {
  var bodyStream = new BinaryInputStream(request.bodyInputStream);
  var bodyBytes = [];
  let bodyAvail;
  while ((bodyAvail = bodyStream.available()) > 0) {
    Array.prototype.push.apply(bodyBytes, bodyStream.readByteArray(bodyAvail));
  }

  var bos = new BinaryOutputStream(response.bodyOutputStream);

  response.processAsync();

  var part = bodyBytes.splice(0, 256);
  bos.writeByteArray(part);

  response.timer1 = new Timer(
    function (timer) {
      bos.writeByteArray(bodyBytes);
    },
    1000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );

  response.timer2 = new Timer(
    function (timer) {
      response.finish();
    },
    2000,
    Ci.nsITimer.TYPE_ONE_SHOT
  );
}
