var proxyToArray = new Proxy(Array, {});
new proxyToArray();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
