// Binary: cache/js-dbg-32-7d06dac3fe83-linux
// Flags: -j
//
function partOfSHA1(str)
{
  var rotate_left = function (n,s) { return ( n<<s ) | (n>>>(32-s)); },
  W = [], H0 = 0x67452301,
  H1 = 0xEFCDAB89, H2 = 0x98BADCFE,
  H3 = 0x10325476, H4 = 0xC3D2E1F0,
  A, B, C, D, E, temp, str_len = str.length,
  word_array = [];
  i = 0x080000000;
  word_array.push( (str_len<<3)&0x0ffffffff );
  for ( blockstart=0; blockstart<word_array.length; blockstart+=16 ) {
    A = H0;
    B = H1;
    C = H2;
    D = H3;
    E = H4;
    for (i= 0; i<=19; ++i) {
      temp = (rotate_left(A,5) + ((B&C) | (~B&D)) + E + W[i] + 0x5A827999) & 0x0ffffffff;
      E = D;
      D = C;
      C = rotate_left(B,30);
      B = A;
      A = temp;
    }
  }
}

partOfSHA1(1226369254122);
