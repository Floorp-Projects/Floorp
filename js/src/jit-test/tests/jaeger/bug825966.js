datediff = function(date1, date2, interval) {
    var delta = 1;
    switch(interval) {
      case "day":
	delta /= 24;
      case "minute":
	delta /= 60;
      case Math:
	break;
    }
    return delta;
};

var diff = datediff(new Date("2012-04-28T14:30:00Z"), new Date("2012-04-29T14:30:00Z"), "day");
for (var i = 0; i < 50; i++) {
    diff = datediff(new Date("2012-04-28T17:00:00Z"), new Date("2012-04-28T17:30:00Z"), "minute");
    assertEq(diff, 1/60);
}
