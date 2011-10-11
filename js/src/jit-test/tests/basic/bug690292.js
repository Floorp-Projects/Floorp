
done = false;
try {
    function x() {}
    print(this.watch("d", Object.create))
    var d = <x></x>
} catch (e) {}
try {
  eval("d = ''")
  done = true;
} catch (e) {}
assertEq(done, false);
