// |jit-test| --enable-import-attributes

async function test() {
    try {
      await import('./not-a-real-file.json', {with:{'unsupportedAttributeKey': 'json'}});
      throw new Error("unreachable");
    } catch (error) {
      assertEq(error instanceof TypeError, true);
      assertEq(error.message, "Unsupported import attribute: unsupportedAttributeKey");
    }

    try {
      await import('./not-a-real-file.json', {with:{'unsupportedAttributeKey': 'json', type: 'json'}});
      throw new Error("unreachable");
    } catch (error) {
      assertEq(error instanceof TypeError, true);
      assertEq(error.message, "Unsupported import attribute: unsupportedAttributeKey");
    }

    try {
      await import('./not-a-real-file.json', {with:{type: 'json', 'unsupportedAttributeKey': 'json'}});
      throw new Error("unreachable");
    } catch (error) {
      assertEq(error instanceof TypeError, true);
      assertEq(error.message, "Unsupported import attribute: unsupportedAttributeKey");
    }
}

test();