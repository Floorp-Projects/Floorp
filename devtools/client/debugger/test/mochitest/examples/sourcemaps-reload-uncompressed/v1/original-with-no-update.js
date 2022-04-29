window.baabar = function baabar() {
  return new Promise(resolve => setTimeout(resolve, 100))
}

window.foobar = async function foobar() {
  await baabar();
  console.log("YO")
}
foobar();
