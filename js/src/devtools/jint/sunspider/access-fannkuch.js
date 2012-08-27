/* The Great Computer Language Shootout
   http://shootout.alioth.debian.org/
   contributed by Isaac Gouy */

function fannkuch(n) {
   var check = 0;
   var perm = Array(n);
   var perm1 = Array(n);
   var count = Array(n);
   var maxPerm = Array(n);
   var maxFlipsCount = 0;
   var m = n - 1;

   /* BEGIN LOOP */
   for (var i = 0; i < n; i++) perm1[i] = i;
   /* END LOOP */
   var r = n;

   /* BEGIN LOOP */
   while (true) {
      // write-out the first 30 permutations
      if (check < 30){
         var s = "";
	 /* BEGIN LOOP */
         for(var i=0; i<n; i++) s += (perm1[i]+1).toString();
	 /* END LOOP */
         check++;
      }

      /* BEGIN LOOP */
      while (r != 1) { count[r - 1] = r; r--; }
      /* END LOOP */
      if (!(perm1[0] == 0 || perm1[m] == m)) {
	 /* BEGIN LOOP */
         for (var i = 0; i < n; i++) perm[i] = perm1[i];
	 /* END LOOP */

         var flipsCount = 0;
         var k;

	 /* BEGIN LOOP */
         while (!((k = perm[0]) == 0)) {
            var k2 = (k + 1) >> 1;
	    /* BEGIN LOOP */
            for (var i = 0; i < k2; i++) {
               var temp = perm[i]; perm[i] = perm[k - i]; perm[k - i] = temp;
            }
	    /* END LOOP */
            flipsCount++;
         }
	 /* END LOOP */

         if (flipsCount > maxFlipsCount) {
            maxFlipsCount = flipsCount;
	    /* BEGIN LOOP */
            for (var i = 0; i < n; i++) maxPerm[i] = perm1[i];
	    /* END LOOP */
         }
      }

      /* BEGIN LOOP */
      while (true) {
         if (r == n) return maxFlipsCount;
         var perm0 = perm1[0];
         var i = 0;
	 /* BEGIN LOOP */
         while (i < r) {
            var j = i + 1;
            perm1[i] = perm1[j];
            i = j;
         }
	 /* END LOOP */
         perm1[r] = perm0;

         count[r] = count[r] - 1;
         if (count[r] > 0) break;
         r++;
      }
      /* END LOOP */
   }
   /* END LOOP */
}

var n = 8;
var ret = fannkuch(n);

