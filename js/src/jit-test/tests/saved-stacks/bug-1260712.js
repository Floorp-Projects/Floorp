low = high = newGlobal({
  principal: 5
})
high.low = low
high.eval("function a() { return saveStack(1, low) }")
set = eval("high.a()")
serialize(set)
