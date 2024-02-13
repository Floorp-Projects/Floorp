/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests `UrlbarUtils.copySnakeKeysToCamel()`.

"use strict";

add_task(async function noSnakes() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      foo: "foo key",
      bar: "bar key",
    }),
    {
      foo: "foo key",
      bar: "bar key",
    }
  );
});

add_task(async function oneSnake() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      foo: "foo key",
      snake_key: "snake key",
      bar: "bar key",
    }),
    {
      foo: "foo key",
      snake_key: "snake key",
      bar: "bar key",
      snakeKey: "snake key",
    }
  );
});

add_task(async function manySnakeKeys() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      foo: "foo key",
      snake_one: "snake key 1",
      bar: "bar key",
      and_snake_two_also: "snake key 2",
      snake_key_3: "snake key 3",
      snake_key_4_too: "snake key 4",
    }),
    {
      foo: "foo key",
      snake_one: "snake key 1",
      bar: "bar key",
      and_snake_two_also: "snake key 2",
      snake_key_3: "snake key 3",
      snake_key_4_too: "snake key 4",
      snakeOne: "snake key 1",
      andSnakeTwoAlso: "snake key 2",
      snakeKey3: "snake key 3",
      snakeKey4Too: "snake key 4",
    }
  );
});

add_task(async function singleChars() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      a: "a key",
      b_c: "b_c key",
      d_e_f: "d_e_f key",
      g_h_i_j: "g_h_i_j key",
    }),
    {
      a: "a key",
      b_c: "b_c key",
      d_e_f: "d_e_f key",
      g_h_i_j: "g_h_i_j key",
      bC: "b_c key",
      dEF: "d_e_f key",
      gHIJ: "g_h_i_j key",
    }
  );
});

add_task(async function numbers() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      snake_1: "snake 1 key",
      snake_2_too: "snake 2 key",
      "3_snakes": "snake 3 key",
    }),
    {
      snake_1: "snake 1 key",
      snake_2_too: "snake 2 key",
      "3_snakes": "snake 3 key",
      snake1: "snake 1 key",
      snake2Too: "snake 2 key",
      "3Snakes": "snake 3 key",
    }
  );
});

add_task(async function leadingUnderscores() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      _foo: "foo key",
      __bar: "bar key",
      _snake_with_leading: "snake key 1",
      __snake_with_two_leading: "snake key 2",
    }),
    {
      _foo: "foo key",
      __bar: "bar key",
      _snake_with_leading: "snake key 1",
      __snake_with_two_leading: "snake key 2",
      _snakeWithLeading: "snake key 1",
      __snakeWithTwoLeading: "snake key 2",
    }
  );
});

add_task(async function trailingUnderscores() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      foo_: "foo key",
      bar__: "bar key",
      snake_with_trailing_: "snake key 1",
      snake_with_two_trailing__: "snake key 2",
    }),
    {
      foo_: "foo key",
      bar__: "bar key",
      snake_with_trailing_: "snake key 1",
      snake_with_two_trailing__: "snake key 2",
      snakeWithTrailing_: "snake key 1",
      snakeWithTwoTrailing__: "snake key 2",
    }
  );
});

add_task(async function leadingAndTrailingUnderscores() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({
      _foo_: "foo key",
      _extra_long_snake_: "snake key",
    }),
    {
      _foo_: "foo key",
      _extra_long_snake_: "snake key",
      _extraLongSnake_: "snake key",
    }
  );
});

add_task(async function consecutiveUnderscores() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel({ weird__snake: "snake key" }),
    {
      weird__snake: "snake key",
      weird_Snake: "snake key",
    }
  );
});

add_task(async function nested() {
  let obj = UrlbarUtils.copySnakeKeysToCamel({
    foo: "foo key",
    nested: {
      bar: "bar key",
      baz: {
        snake_in_baz: "snake_in_baz key",
      },
      snake_in_nested: {
        snake_in_snake_in_nested: "snake_in_snake_in_nested key",
      },
    },
    snake_key: {
      snake_in_snake_key: "snake_in_snake_key key",
    },
  });

  Assert.equal(obj.foo, "foo key");
  Assert.equal(obj.nested.bar, "bar key");
  Assert.deepEqual(obj.nested.baz, {
    snake_in_baz: "snake_in_baz key",
    snakeInBaz: "snake_in_baz key",
  });
  Assert.deepEqual(obj.nested.snake_in_nested, {
    snake_in_snake_in_nested: "snake_in_snake_in_nested key",
    snakeInSnakeInNested: "snake_in_snake_in_nested key",
  });
  Assert.equal(obj.nested.snake_in_nested, obj.nested.snakeInNested);
  Assert.deepEqual(obj.snake_key, {
    snake_in_snake_key: "snake_in_snake_key key",
    snakeInSnakeKey: "snake_in_snake_key key",
  });
  Assert.equal(obj.snake_key, obj.snakeKey);
});

add_task(async function noOverwrite_ok() {
  Assert.deepEqual(
    UrlbarUtils.copySnakeKeysToCamel(
      {
        foo: "foo key",
        snake_key: "snake key",
      },
      false
    ),
    {
      foo: "foo key",
      snake_key: "snake key",
      snakeKey: "snake key",
    }
  );
});

add_task(async function noOverwrite_throws() {
  Assert.throws(
    () =>
      UrlbarUtils.copySnakeKeysToCamel(
        {
          snake_key: "snake key",
          snakeKey: "snake key",
        },
        false
      ),
    /Can't copy snake_case key/
  );
});
