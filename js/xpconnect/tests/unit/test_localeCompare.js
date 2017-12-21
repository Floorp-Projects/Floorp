function run_test() {
  Assert.ok("C".localeCompare("D") < 0);
  Assert.ok("D".localeCompare("C") > 0);
  Assert.ok("\u010C".localeCompare("D") < 0);
  Assert.ok("D".localeCompare("\u010C") > 0);
}
