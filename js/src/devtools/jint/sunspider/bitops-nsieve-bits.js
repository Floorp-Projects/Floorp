// The Great Computer Language Shootout
//  http://shootout.alioth.debian.org
//
//  Contributed by Ian Osgood

function pad(n,width) {
  var s = n.toString();
  /* BEGIN LOOP */
  while (s.length < width) s = ' ' + s;
  /* END LOOP */
  return s;
}

function primes(isPrime, n) {
  var i, count = 0, m = 10000<<n, size = m+31>>5;

  /* BEGIN LOOP */
  for (i=0; i<size; i++) isPrime[i] = 0xffffffff;
  /* END LOOP */

  /* BEGIN LOOP */
  for (i=2; i<m; i++)
    if (isPrime[i>>5] & 1<<(i&31)) {
      /* BEGIN LOOP */
      for (var j=i+i; j<m; j+=i)
        isPrime[j>>5] &= ~(1<<(j&31));
      /* END LOOP */
      count++;
    }
  /* END LOOP */
}

function sieve() {
    /* BEGIN LOOP */
    for (var i = 4; i <= 4; i++) {
        var isPrime = new Array((10000<<i)+31>>5);
        primes(isPrime, i);
    }
    /* END LOOP */
}

sieve();
