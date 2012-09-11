function reportCompare(actual) - ++actual + "'";
var UBound = 0;
var actualvalues = [];
for (var li = 0; li < 6; ++li) addThis();
function addThis() {
  UBound++;
  for (var i=0; i<UBound; i++)  {
    reportCompare(actualvalues[i]);
  }
}
for (var li = 0; li < 3; ++li) addThis();
