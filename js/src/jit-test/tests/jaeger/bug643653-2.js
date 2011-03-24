var HEAP, IHEAP, FHEAP;
var TOTAL_MEMORY = 50 * 1024 * 1024;
HEAP = IHEAP = new Int32Array(TOTAL_MEMORY);
STACK_ROOT = STACKTOP = undefined;
var _rng;
var __str2;
var __str3;
{
    var __stackBase__ = STACKTOP;
    var $n;
    var $tmp5 = __stackBase__ + 12;
    var $tmp6 = $n;
    var $mul7 = ($tmp6) * 3;
    $this_addr_i23 = $tmp5;
    $id_addr_i = __str2;
    $desc_addr_i = __str3;
    $N_addr_i = $mul7;
    var $this1_i24 = $this_addr_i23;
    var $tmp_i25 = $id_addr_i;
    var $tmp2_i = $desc_addr_i;
    var $tmp3_i = $N_addr_i;
    __Z9makeFastaI10RandomizedEvPKcS2_jRT_($tmp_i25, $tmp2_i, $tmp3_i, $this1_i24);
}
function __Z9makeFastaI10RandomizedEvPKcS2_jRT_($id, $desc, $N, $output)
{
    $output_addr = $output;
    var $tmp4 = $output_addr;
    $this_addr_i = $tmp4;
    var $this1_i = $this_addr_i;
    var $table_i = $this1_i;
    var $call_i = __ZN10LineBuffer7genrandER10Cumulativej(0, $table_i, 0);
}
function __ZN10LineBuffer7genrandER10Cumulativej($this, $table, $N)
{
    var $this_addr_i1;
    var $pct_addr_i;
    $table_addr = $table;
    var $tmp3 = $table_addr;
    $this_addr_i = _rng;
    $max_addr_i = 1;
    var $this1_i = $this_addr_i;
    var $last_i = $this1_i;
    var $tmp_i = IHEAP[$last_i];
    var $mul_i = ($tmp_i) * 3877;
    var $add_i = ($mul_i) + 29573;
    var $rem_i = ($add_i) % 139968;
    var $last2_i = $this1_i;
    IHEAP[$last2_i] = $rem_i;
    var $tmp3_i = $max_addr_i;
    var $last4_i = $this1_i;
    var $tmp5_i = IHEAP[$last4_i];
    var $conv_i = ($tmp5_i);
    var $mul6_i = ($tmp3_i) * ($conv_i);
    var $div_i = ($mul6_i) / 139968;
    $this_addr_i1 = $tmp3;
    $pct_addr_i = $div_i;
    assertEq($pct_addr_i, NaN);
}
