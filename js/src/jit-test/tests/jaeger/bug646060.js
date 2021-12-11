function f0(i) {
  switch(i) {
  case "a":
  case { TITLE: false, VERSION: false }('test')
 : 
  }
}
try { new TestCase(SECTION, 'switch statement', f0("a"), "ab*"); } catch (e) {}
