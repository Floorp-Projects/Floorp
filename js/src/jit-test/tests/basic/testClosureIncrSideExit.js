function testClosureIncrSideExit() {
    {
      let f = function (y) {
        {
          let ff = function (g) {
            for each(let h in g) {
                if (++y > 5) {
                    return 'ddd';
                }
            }
            return 'qqq';
        };
            return ff(['', null, '', false, '', '', null]);
        }
      };
        return f(-1);
    }
}
assertEq(testClosureIncrSideExit(), "ddd");
