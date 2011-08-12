var s1 = 'xx';
for (var x = 0; x < 10 ; ++x ) { 
  new function() s1++;
  gc();
}
