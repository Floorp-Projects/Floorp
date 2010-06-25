function echo() {
  sendMessage.apply(this, arguments);
}

registerReceiver("echo", echo);

registerReceiver("callback",
                 function(msgName, data, handle) {
                   sendMessage("sendback",
                               callMessage("callback", data)[0],
                               handle);
                 });

registerReceiver("gimmeHandle",
                 function(msgName) {
                   sendMessage("recvHandle", "ok", createHandle());
                 });

registerReceiver("kthx",
                 function(msgName, data, child) {
                   sendMessage("recvHandleAgain", data + data, child.parent);
                 });

registerReceiver("echo2", echo);

registerReceiver("multireturn begin",
                 function() {
                   var results = callMessage("multireturn");
                   sendMessage.apply(null, ["multireturn check"].concat(results));
                 });

registerReceiver("testarray",
                 function(msgName, array) {
                   sendMessage("testarray", array.reverse());
                 });

registerReceiver("test primitive types", echo);

registerReceiver("drop methods", echo);

registerReceiver("exception coping", echo);

registerReceiver("duplicate receivers", echo);
