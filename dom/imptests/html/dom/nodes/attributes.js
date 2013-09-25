function attr_is(attr, v, ln, ns, p, n) {
  assert_equals(attr.value, v)
  assert_equals(attr.localName, ln)
  assert_equals(attr.namespaceURI, ns)
  assert_equals(attr.prefix, p)
  assert_equals(attr.name, n)
}

function attributes_are(el, l) {
  for (var i = 0, il = l.length; i < il; i++) {
    attr_is(el.attributes[i], l[i][1], l[i][0], (l[i].length < 3) ? null : l[i][2], null, l[i][0])
//    assert_equals(el.attributes[i].ownerElement, el)
  }
}
