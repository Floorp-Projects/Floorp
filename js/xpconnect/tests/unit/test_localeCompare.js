function run_test() {
  do_check_true("C".localeCompare("D") < 0);
  do_check_true("D".localeCompare("C") > 0);
  do_check_true("\u010C".localeCompare("D") < 0);
  do_check_true("D".localeCompare("\u010C") > 0);
}
