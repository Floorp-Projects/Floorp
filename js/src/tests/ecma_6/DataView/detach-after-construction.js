// |reftest| skip-if(!xulRuntime.shell) -- needs detachArrayBuffer

var buf = new ArrayBuffer([1,2]);
var bufView = new DataView(buf);

detachArrayBuffer(buf);

assertThrowsInstanceOf(() => bufView.getInt8(0), TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0, 0, "OK");
