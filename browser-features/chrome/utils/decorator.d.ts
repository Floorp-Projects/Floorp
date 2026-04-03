// from https://github.com/tc39/proposal-decorators/blob/8936bfe642c63db0c6565572c4847c120400dac3/README.md

export type ClassMethodDecorator = (
  // deno-lint-ignore ban-types
  value: Function,
  context: {
    kind: "method";
    name: string | symbol;
    access: { get(): unknown };
    static: boolean;
    private: boolean;
    addInitializer(initializer: () => void): void;
  },
// deno-lint-ignore ban-types
) => Function | void;

export type ClassGetterDecorator = (
  // deno-lint-ignore ban-types
  value: Function,
  context: {
    kind: "getter";
    name: string | symbol;
    access: { get(): unknown };
    static: boolean;
    private: boolean;
    addInitializer(initializer: () => void): void;
  },
// deno-lint-ignore ban-types
) => Function | void;

export type ClassSetterDecorator = (
  // deno-lint-ignore ban-types
  value: Function,
  context: {
    kind: "setter";
    name: string | symbol;
    access: { set(value: unknown): void };
    static: boolean;
    private: boolean;
    addInitializer(initializer: () => void): void;
  },
// deno-lint-ignore ban-types
) => Function | void;

export type ClassFieldDecorator = (
  value: undefined,
  context: {
    kind: "field";
    name: string | symbol;
    access: { get(): unknown; set(value: unknown): void };
    static: boolean;
    private: boolean;
    addInitializer(initializer: () => void): void;
  },
) => (initialValue: unknown) => unknown | void;

// deno-lint-ignore no-explicit-any
export type ClassDecorator<T = any> = <
  // deno-lint-ignore no-explicit-any
  Class extends new (...args: any) => T = new (...args: any) => T,
>(
  value: Class,
  context: ClassDecoratorContext<Class>,
) => Class | void;

export type ClassAutoAccessorDecorator = (
  value: {
    get: () => unknown;
    set: (value: unknown) => void;
  },
  context: {
    kind: "accessor";
    name: string | symbol;
    access: { get(): unknown; set(value: unknown): void };
    static: boolean;
    private: boolean;
    addInitializer(initializer: () => void): void;
  },
) => {
  get?: () => unknown;
  set?: (value: unknown) => void;
  init?: (initialValue: unknown) => unknown;
} | void;
