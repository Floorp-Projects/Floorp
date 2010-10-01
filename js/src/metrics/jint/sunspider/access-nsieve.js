// The Great Computer Language Shootout
// http://shootout.alioth.debian.org/
//
// modified by Isaac Gouy

function pad(number,width){
   var s = number.toString();
   var prefixWidth = width - s.length;
   if (prefixWidth>0){
      /* BEGIN LOOP */
      for (var i=1; i<=prefixWidth; i++) s = " " + s;
      /* END LOOP */
   }
   return s;
}

function nsieve(m, isPrime){
   var i, k, count;

   /* BEGIN LOOP */
   for (i=2; i<=m; i++) { isPrime[i] = true; }
   /* END LOOP */
   count = 0;

   /* BEGIN LOOP */
   for (i=2; i<=m; i++){
      if (isPrime[i]) {
	 /* BEGIN LOOP */
         for (k=i+i; k<=m; k+=i) isPrime[k] = false;
	 /* END LOOP */
         count++;
      }
   }
   /* END LOOP */
   return count;
}

function sieve() {
    /* BEGIN LOOP */
    for (var i = 1; i <= 3; i++ ) {
        var m = (1<<i)*10000;
        var flags = Array(m+1);
        nsieve(m, flags);
    }
    /* END LOOP */
}

sieve();
