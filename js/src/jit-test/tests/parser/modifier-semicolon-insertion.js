Reflect.parse(`
function a()f1()
f2()
`);
Reflect.parse(`
let a
f2()
`);
Reflect.parse(`
let a=1
f2()
`);
Reflect.parse(`
import 'a'
f2()
`, {target: "module"});
Reflect.parse(`
export { a } from 'a'
f2()
`, {target: "module"});
Reflect.parse(`
var a
f2()
`);
Reflect.parse(`
var a=1
f2()
`);
Reflect.parse(`
f1()
f2()
`);
Reflect.parse(`
while(false) { continue
f2() }
`);
Reflect.parse(`
while(false) { break
f2() }
`);
Reflect.parse(`
function a() { return
f2() }
`);
Reflect.parse(`
throw 1
f2()
`);
Reflect.parse(`
debugger
f2()
`);
