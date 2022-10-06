// |reftest| skip-if(!this.hasOwnProperty('settlePromiseNow'))

function main() {
    function v0(v1) {
        throw "foobar";
    }
    const v8 = new Promise(v0);
    const v9 = v8.catch();
    const v11 = this.settlePromiseNow(v9);
    function v12(v13) {
    }
    const v15 = new Promise(v11);
    const v16 = v15.catch(v12);
    gc();
}
main();

this.reportCompare && reportCompare(true, true);
