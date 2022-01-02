// |jit-test| error: TypeError
function BigInteger(a,b,c) {
  this.array = new Array();
  if(a != null)
    if("number" == typeof a) this.fromNumber(a,b,c);
    else this.fromString(a,b);
}
function nbi() { return new BigInteger(null); }
function am3(i,x,w,j,c,n) {}
setupEngine = function(fn, bits) {
  dbits = bits;
  BI_DB = dbits;
  BI_DM = ((1<<dbits)-1);
}
function intAt(s,i) {}
function bnpFromInt(x) {}
function nbv(i) { var r = nbi(); r.fromInt(i); return r; }
function bnpFromString(s,b) {
  var this_array = this.array;
  if(b == 16) k = 4;
  this.t = 0;
  var i = s.length, mi = false, sh = 0;
  while(--i >= 0) {
    var x = (k==8)?s[i]&0xff:intAt(s,i);
    if(sh == 0)
      this_array[this.t++] = x;
    else if(sh+k > BI_DB) {
      this_array[this.t++] = (x>>(BI_DB-sh));
    }
    sh += k;
  }
}
function bnAbs() { return (this.s<0)?this.negate():this; }
function nbits(x) {
  var r = 1, t;
  return r;
}
function bnBitLength() {}
function bnpDLShiftTo(n,r) {
  var this_array = this.array;
  var r_array = r.array;
  for(i = this.t-1; i >= 0; --i) r_array[i+n] = this_array[i];
  r.t = this.t+n;
}
function bnpLShiftTo(n,r) {
  var bs = n%BI_DB;
  var ds = Math.floor(n/BI_DB), c = (this.s<<bs)&BI_DM, i;
  r.t = this.t+ds+1;
}
function bnpDivRemTo(m,q,r) {
  var pm = m.abs();
  var pt = this.abs();
  var y = nbi(), ts = this.s, ms = m.s;
  var pm_array = pm.array;
  var nsh = BI_DB-nbits(pm_array[pm.t-1]);
  if(nsh > 0) { pm.lShiftTo(nsh,y); pt.lShiftTo(nsh,r); }
  var ys = y.t;
  var i = r.t, j = i-ys, t = (q==null)?nbi():q;
  y.dlShiftTo(j,t);
  BigInteger.ONE.dlShiftTo(ys,t);
}
function bnMod(a) {
  var r = nbi();
  this.abs().divRemTo(a,null,r);
}
function Montgomery(m) {
  this.m = m;
}
function montConvert(x) {
  var r = nbi();
  x.abs().dlShiftTo(this.m.t,r);
  r.divRemTo(this.m,null,r);
}
function montRevert(x) {
  var r = nbi();
  return r;
}
Montgomery.prototype.convert = montConvert;
Montgomery.prototype.revert = montRevert;
function bnpIsEven() {}
function bnpExp(e,z) {
  var r = nbi(), r2 = nbi(), g = z.convert(this), i = nbits(e)-1;
  return z.revert(r);
}
function bnModPowInt(e,m) {
  if(e < 256 || m.isEven()) z = new Classic(m); else z = new Montgomery(m);
  return this.exp(e,z);
}
BigInteger.prototype.fromInt = bnpFromInt;
BigInteger.prototype.fromString = bnpFromString;
BigInteger.prototype.dlShiftTo = bnpDLShiftTo;
BigInteger.prototype.lShiftTo = bnpLShiftTo;
BigInteger.prototype.divRemTo = bnpDivRemTo;
BigInteger.prototype.isEven = bnpIsEven;
BigInteger.prototype.exp = bnpExp;
BigInteger.prototype.abs = bnAbs;
BigInteger.prototype.bitLength = bnBitLength;
BigInteger.prototype.mod = bnMod;
BigInteger.prototype.modPowInt = bnModPowInt;
BigInteger.ONE = nbv(1);
function parseBigInt(str,r) {
  return new BigInteger(str,r);
}
function pkcs1pad2(s,n) {
  var ba = new Array();
  return new BigInteger(ba);
}
function RSAKey() {
}
function RSASetPublic(N,E) {
    this.n = parseBigInt(N,16);
}
function RSADoPublic(x) {
  return x.modPowInt(this.e, this.n);
}
function RSAEncrypt(text) {
  var m = pkcs1pad2(text,(this.n.bitLength()+7)>>3);
  var c = this.doPublic(m);
  var h = c.toString(16);
  if((h.length & 1) == 0) return h; else return "0" + h;
}
RSAKey.prototype.doPublic = RSADoPublic;
RSAKey.prototype.setPublic = RSASetPublic;
RSAKey.prototype.encrypt = RSAEncrypt;
function RSASetPrivateEx(N,E,D,P,Q,DP,DQ,C) {
    this.p = parseBigInt(P,16);
}
function RSADoPrivate(x) {
  var xp = x.mod(this.p).modPow(this.dmp1, this.p);
}
function RSADecrypt(ctext) {
  var c = parseBigInt(ctext, 16);
  var m = this.doPrivate(c);
}
RSAKey.prototype.doPrivate = RSADoPrivate;
RSAKey.prototype.setPrivateEx = RSASetPrivateEx;
RSAKey.prototype.decrypt = RSADecrypt;
nValue="a5261939975948bb7a58dffe5ff54e65f0498f9175f5a09288810b8975871e99af3b5dd94057b0fc07535f5f97444504fa35169d461d0d30cf0192e307727c065168c788771c561a9400fb49175e9e6aa4e23fe11af69e9412dd23b0cb6684c4c2429bce139e848ab26d0829073351f4acd36074eafd036a5eb83359d2a698d3";
eValue="10001";
dValue="8e9912f6d3645894e8d38cb58c0db81ff516cf4c7e5a14c7f1eddb1459d2cded4d8d293fc97aee6aefb861859c8b6a3d1dfe710463e1f9ddc72048c09751971c4a580aa51eb523357a3cc48d31cfad1d4a165066ed92d4748fb6571211da5cb14bc11b6e2df7c1a559e6d5ac1cd5c94703a22891464fba23d0d965086277a161";
pValue="d090ce58a92c75233a6486cb0a9209bf3583b64f540c76f5294bb97d285eed33aec220bde14b2417951178ac152ceab6da7090905b478195498b352048f15e7d";
qValue="cab575dc652bb66df15a0359609d51d1db184750c00c6698b90ef3465c99655103edbf0d54c56aec0ce3c4d22592338092a126a0cc49f65a4a30d222b411e58f";
dmp1Value="1a24bca8e273df2f0e47c199bbf678604e7df7215480c77c8db39f49b000ce2cf7500038acfff5433b7d582a01f1826e6f4d42e1c57f5e1fef7b12aabc59fd25";
dmq1Value="3d06982efbbe47339e1f6d36b1216b8a741d410b0c662f54f7118b27b9a4ec9d914337eb39841d8666f3034408cf94f5b62f11c402fc994fe15a05493150d9fd";
coeffValue="3a3e731acd8960b7ff9eb81a7ff93bd1cfa74cbd56987db58b4594fb09c09084db1734c8143f98b602b981aaa9243ca28deb69b5b280ee8dcee0fd2625e53250";
setupEngine(am3, 28);
function check_correctness(text, hash) {
  var RSA = new RSAKey();
  RSA.setPublic(nValue, eValue);
  RSA.setPrivateEx(nValue, eValue, dValue, pValue, qValue, dmp1Value, dmq1Value, coeffValue);
  var encrypted = RSA.encrypt(text);
  var decrypted = RSA.decrypt(encrypted);
}
check_correctness("Hello! I am some text.", "142b19b40fee712ab9468be296447d38c7dfe81a7850f11ae6aa21e49396a4e90bd6ba4aa385105e15960a59f95447dfad89671da6e08ed42229939583753be84d07558abb4feee4d46a92fd31d962679a1a5f4bf0fb7af414b9a756e18df7e6d1e96971cc66769f3b27d61ad932f2211373e0de388dc040557d4c3c3fe74320");
