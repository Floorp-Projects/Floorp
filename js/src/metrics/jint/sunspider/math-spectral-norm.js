// The Great Computer Language Shootout
// http://shootout.alioth.debian.org/
//
// contributed by Ian Osgood

function A(i,j) {
  return 1/((i+j)*(i+j+1)/2+i+1);
}

function Au(u,v) {
  /* BEGIN LOOP */
  for (var i=0; i<u.length; ++i) {
    var t = 0;
  /* BEGIN LOOP */
    for (var j=0; j<u.length; ++j)
      t += A(i,j) * u[j];
  /* END LOOP */
    v[i] = t;
  }
  /* END LOOP */
}

function Atu(u,v) {
  /* BEGIN LOOP */
  for (var i=0; i<u.length; ++i) {
    var t = 0;
  /* BEGIN LOOP */
    for (var j=0; j<u.length; ++j)
      t += A(j,i) * u[j];
  /* END LOOP */
    v[i] = t;
  }
  /* END LOOP */
}

function AtAu(u,v,w) {
  Au(u,w);
  Atu(w,v);
}

function spectralnorm(n) {
  var i, u=[], v=[], w=[], vv=0, vBv=0;
  /* BEGIN LOOP */
  for (i=0; i<n; ++i) {
    u[i] = 1; v[i] = w[i] = 0;
  }
  /* END LOOP */
  /* BEGIN LOOP */
  for (i=0; i<10; ++i) {
    AtAu(u,v,w);
    AtAu(v,u,w);
  }
  /* END LOOP */
  /* BEGIN LOOP */
  for (i=0; i<n; ++i) {
    vBv += u[i]*v[i];
    vv  += v[i]*v[i];
  }
  /* END LOOP */
  return Math.sqrt(vBv/vv);
}

  /* BEGIN LOOP */
for (var i = 6; i <= 48; i *= 2) {
    spectralnorm(i);
}
  /* END LOOP */
