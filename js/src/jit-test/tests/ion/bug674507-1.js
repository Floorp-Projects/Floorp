function branchy(v0,v1) {
 var result = 0;
 if (v0) {
  if (v0 & v0) {
   if (v1 & v0) {
   } else {
    result = v0 & v0 & v1;
   }
  } else {
   if (v0 & v0 & v0) {
    result = v1;
   }
  }
 } else {
   if (v0 & v1 & v0) { }
 }
 return result;
}
branchy(932,256,368)
