add_task(async function helper() {
  // Note: this test assumes the largest possible ArrayBuffer is
  // smaller than 10GB -- if that changes, this test will fail.
  let rs = new ReadableStream({
    type: "bytes",
    autoAllocateChunkSize: 10 * 1024 * 1024 * 1024,
  });
  let reader = rs.getReader();
  try {
    await reader.read();
    Assert.equal(true, false, "Shouldn't succeed at reading");
  } catch (e) {
    Assert.equal(e instanceof RangeError, true, "Should throw RangeError");
  }
});
