// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-32-47a8311cf0bb-linux
// Flags:
//
x = /x/
Function("Object.defineProperty(x,new AttributeName,({e:true,enumerable:true}))")()
{
  throw (Object.keys)(x, /x/)
}
