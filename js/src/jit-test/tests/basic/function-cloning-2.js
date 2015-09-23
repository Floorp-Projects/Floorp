if (typeof oomAtAllocation === 'object') {
    var a = [];
    oomAtAllocation(1);
    try {
    	a.forEach();
    } catch (e) {
    }
    a.forEach(()=>1);
}
