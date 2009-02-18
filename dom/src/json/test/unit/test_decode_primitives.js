function parse_primitives() {

  print("parse object");
  // check an empty object, just for sanity
  var emptyObject = "{}";
  var x = nativeJSON.decode(emptyObject);
  do_check_eq(typeof x, "object");
  print("parse object 2");
  
  x = JSON.parse(emptyObject);
  do_check_eq(typeof x, "object");
  
  print("parse object 3");
  x = crockfordJSON.parse(emptyObject);
  do_check_eq(typeof x, "object");

  // booleans and null
  print("parse bool");
  x = JSON.parse("true");
  do_check_eq(typeof x, "boolean");
  do_check_eq(x, true);
  
  print("parse bool with space")
  x = JSON.parse("true          ");
  do_check_eq(typeof x, "boolean");
  do_check_eq(x, true);
  
  print("parse false");
  x = JSON.parse("false");
  do_check_eq(typeof x, "boolean");
  do_check_eq(x, false);
  
  print("parse null");
  x = JSON.parse("           null           ");
  do_check_eq(typeof x, "object");
  do_check_eq(x, null);
  
  // numbers
  print("parse numbers")
  x = JSON.parse("1234567890");
  do_check_eq(typeof x, "number");
  do_check_eq(x, 1234567890);
  
  x = JSON.parse("-9876.543210");
  do_check_eq(typeof x, "number");
  do_check_eq(x, -9876.543210);
  
  x = JSON.parse("0.123456789e-12");
  do_check_eq(typeof x, "number");
  do_check_eq(x, 0.123456789e-12);
  
  x = JSON.parse("1.234567890E+34");
  do_check_eq(typeof x, "number");
  do_check_eq(x, 1.234567890E+34);

  x = JSON.parse("      23456789012E66          \r\r\r\r      \n\n\n\n ");
  do_check_eq(typeof x, "number");
  do_check_eq(x, 23456789012E66);
 
  // strings
  x = crockfordJSON.parse('"foo"');
  do_check_eq(typeof x, "string");
  do_check_eq(x, "foo");
  x = JSON.parse('"foo"');
  do_check_eq(typeof x, "string");
  do_check_eq(x, "foo");
  
  x = JSON.parse('"\\r\\n"');
  do_check_eq(typeof x, "string");
  do_check_eq(x, "\r\n");
  
  x = JSON.parse('"\\uabcd\uef4A"');
  do_check_eq(typeof x, "string");
  do_check_eq(x, "\uabcd\uef4A");

  x = JSON.parse('"\\uabcd"');
  do_check_eq(typeof x, "string");
  do_check_eq(x, "\uabcd");

  x = JSON.parse('"\\f"');
  do_check_eq(typeof x, "string");
  do_check_eq(x, "\f");

}

function run_test() {
  parse_primitives();
}
