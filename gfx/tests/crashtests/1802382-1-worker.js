self.onmessage = async function(e) {
  let a = new FontFace('a', 'url(x)')
  let b = new FontFace('b', 'url(x)')
  self.fonts.add(a)
  self.fonts.add(b)
}
