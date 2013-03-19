# Changes

## 1.0.2 / 2012-12-26

  - Delegate to polymorphic methods from `.define` and `.implement` so, they
    can be overidden.

## 1.0.1 / 2012-11-11

  - Fix issues with different `Error` types as they all inherit from
    `Error`.

## 1.0.0 / 2012-11-09

  - Add browser test integration.
  - Fix cross-browser incompatibilities & test failures.
  - Add support for host objects.
  - Add optional `hint` argument for method to ease debugging.
  - Remove default implementation at definition time.

## 0.1.1 / 2012-10-15

 - Fix regression causing custom type implementation to be stored on objects.

## 0.1.0 / 2012-10-15

 - Remove dependency on name module.
 - Implement fallback for engines that do not support ES5.
 - Add support for built-in type extensions without extending their prototypes.
 - Make API for default definitions more intuitive.
   Skipping type argument now defines default:

      isFoo.define(function(value) {
        return false
      })

 - Make exposed `define` and `implement` polymorphic.
 - Removed dev dependency on swank-js.
 - Primitive types `string, number, boolean` no longer inherit method
   implementations from `Object`.

## 0.0.3 / 2012-07-17

  - Remove module boilerplate

## 0.0.2 / 2012-06-26

  - Name changes to make it less conflicting with other library conventions.
  - Expose function version of `define` & `implement` methods.
  - Expose `Null` and `Undefined` object holding implementations for an
    associated types.

## 0.0.1 / 2012-06-25

  - Initial release
