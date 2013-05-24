assertEq((function() {
      for (var i = 0; i < 5; i++) {
        print(i);
      }
      const x = 3;
      (function() x++)();
      return x
})(), 3);
