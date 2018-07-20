if (!this.oomTest) {
    this.reportCompare && reportCompare(true, true);
    quit(0);
}

let lfPreamble = `
`;
oomTest(new Function(`var TOTAL_MEMORY = 50 * 1024 * 1024;
HEAP = IHEAP = new Int32Array(TOTAL_MEMORY);
function __Z9makeFastaI10RandomizedEvPKcS2_jRT_(\$id, \$desc, \$N, \$output)
{
}
`));

this.reportCompare && reportCompare(true, true);
