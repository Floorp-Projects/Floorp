// |jit-test| error:TypeError

// Binary: cache/js-dbg-32-86c8e18f20eb-linux
// Flags:
//
(function(){
  for each (var x in new (
    (function (){x})()
    for each (y in [])
  )
)
{const functional=undefined}
})()
