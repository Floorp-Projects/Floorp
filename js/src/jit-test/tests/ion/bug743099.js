function Day( t ) {}
function WeekDay( t ) {
  var weekday = (Day(t)+4) % 7;
  return( weekday < 0 ? 7 + weekday : weekday );
}
var expect = 'No Error';
for (var i = 0; i < 50; i++) {
    var [] = [expect ? WeekDay(i.a) : true], uneval;
}
