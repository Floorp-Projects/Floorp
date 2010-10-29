bitwiseAndValue = Math.pow(2,32);
for (var i = 0; i < 60000; i++)
  bitwiseAndValue = bitwiseAndValue & i;

assertEq(bitwiseAndValue, 0);



