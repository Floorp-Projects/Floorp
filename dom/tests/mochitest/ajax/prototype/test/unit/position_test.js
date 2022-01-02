var testVar = 'to be updated';

new Test.Unit.Runner({
  
  setup: function() {
    scrollTo(0,0);
    Position.prepare();
    Position.includeScrollOffsets = false;
  },
  
  teardown: function() {
    scrollTo(0,0);
    Position.prepare();
    Position.includeScrollOffsets = false;
  },
  
  testPrepare: function() {
    Position.prepare();
    this.assertEqual(0, Position.deltaX);
    this.assertEqual(0, Position.deltaY);
    scrollTo(20,30);
    Position.prepare();
    this.assertEqual(20, Position.deltaX);
    this.assertEqual(30, Position.deltaY);
  },
  
  testWithin: function() {
    [true, false].each(function(withScrollOffsets) {
      Position.includeScrollOffsets = withScrollOffsets;
      this.assert(!Position.within($('body_absolute'), 9, 9), 'outside left/top');
      this.assert(Position.within($('body_absolute'), 10, 10), 'left/top corner');
      this.assert(Position.within($('body_absolute'), 10, 19), 'left/bottom corner');
      this.assert(!Position.within($('body_absolute'), 10, 20), 'outside bottom');
    }, this);
    
    scrollTo(20,30);
    Position.prepare();
    Position.includeScrollOffsets = true;
    this.assert(!Position.within($('body_absolute'), 9, 9), 'outside left/top');
    this.assert(Position.within($('body_absolute'), 10, 10), 'left/top corner');
    this.assert(Position.within($('body_absolute'), 10, 19), 'left/bottom corner');
    this.assert(!Position.within($('body_absolute'), 10, 20), 'outside bottom');
  }
});