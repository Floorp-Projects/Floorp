// |reftest| skip-if(!xulRuntime.shell)

for (var neuterArg of ['change-data', 'same-data']) {
    var buf = new ArrayBuffer([1,2]);
    var bufView = new DataView(buf);

    neuter(buf, neuterArg);

    assertThrowsInstanceOf(()=>bufView.getInt8(0), TypeError);
}

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
