// |jit-test| 

Reflect.parse(`class A {
  static { print('hi'); }
}`)

Reflect.parse(`class A {
  static x = 10;
  static { this.x++ }
}`);