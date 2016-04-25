function test1() {
  do {
    "8pan08pa8pan08pa".split("");
  } while (!inIon());
}

function test2() {
  do {
    "abababababababababababababababab".split("a");
  } while (!inIon());
}

function test3() {
  do {
    "abcabcabcabcabcabcabcabcabcabcabcabcabcabcabc".split("ab");
  } while (!inIon());
}

function test4() {
  do {
    "".split("");
  } while (!inIon());
}

test1();
test2();
test3();
test4();
