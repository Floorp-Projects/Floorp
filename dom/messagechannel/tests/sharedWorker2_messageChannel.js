var mc = new MessageChannel();
var i = 0;

onconnect = function(evt) {
  dump("CONNECTING: "+ i +"\n");
  evt.ports[0].postMessage(42, [mc['port' + ++i]]);
}
