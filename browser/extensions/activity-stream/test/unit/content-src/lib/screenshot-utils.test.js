import {GlobalOverrider} from "test/unit/utils";
import {ScreenshotUtils} from "content-src/lib/screenshot-utils";

const DEFAULT_BLOB_URL = "blob://test";

describe("ScreenshotUtils", () => {
  let globals;
  let url;
  beforeEach(() => {
    globals = new GlobalOverrider();
    url = {
      createObjectURL: globals.sandbox.stub().returns(DEFAULT_BLOB_URL),
      revokeObjectURL: globals.sandbox.spy()
    };
    globals.set("URL", url);
  });
  afterEach(() => globals.restore());
  describe("#createLocalImageObject", () => {
    it("should return null if no remoteImage is supplied", () => {
      let localImageObject = ScreenshotUtils.createLocalImageObject(null);

      assert.notCalled(url.createObjectURL);
      assert.equal(localImageObject, null);
    });
    it("should create a local image object with the correct properties if remoteImage is a blob", () => {
      let localImageObject = ScreenshotUtils.createLocalImageObject({path: "/path1", data: new Blob([0])});

      assert.calledOnce(url.createObjectURL);
      assert.deepEqual(localImageObject, {path: "/path1", url: DEFAULT_BLOB_URL});
    });
    it("should create a local image object with the correct properties if remoteImage is a normal image", () => {
      const imageUrl = "https://test-url";
      let localImageObject = ScreenshotUtils.createLocalImageObject(imageUrl);

      assert.notCalled(url.createObjectURL);
      assert.deepEqual(localImageObject, {url: imageUrl});
    });
  });
  describe("#maybeRevokeBlobObjectURL", () => {
    // Note that we should also ensure that all the tests for #isBlob are green.
    it("should call revokeObjectURL if image is a blob", () => {
      ScreenshotUtils.maybeRevokeBlobObjectURL({path: "/path1", url: "blob://test"});

      assert.calledOnce(url.revokeObjectURL);
    });
    it("should not call revokeObjectURL if image is not a blob", () => {
      ScreenshotUtils.maybeRevokeBlobObjectURL({url: "https://test-url"});

      assert.notCalled(url.revokeObjectURL);
    });
  });
  describe("#isRemoteImageLocal", () => {
    it("should return true if both propsImage and stateImage are not present", () => {
      assert.isTrue(ScreenshotUtils.isRemoteImageLocal(null, null));
    });
    it("should return false if propsImage is present and stateImage is not present", () => {
      assert.isFalse(ScreenshotUtils.isRemoteImageLocal(null, {}));
    });
    it("should return false if propsImage is not present and stateImage is present", () => {
      assert.isFalse(ScreenshotUtils.isRemoteImageLocal({}, null));
    });
    it("should return true if both propsImage and stateImage are equal blobs", () => {
      const blobPath = "/test-blob-path/test.png";
      assert.isTrue(ScreenshotUtils.isRemoteImageLocal(
        {path: blobPath, url: "blob://test"}, // state
        {path: blobPath, data: new Blob([0])} // props
      ));
    });
    it("should return false if both propsImage and stateImage are different blobs", () => {
      assert.isFalse(ScreenshotUtils.isRemoteImageLocal(
        {path: "/path1", url: "blob://test"}, // state
        {path: "/path2", data: new Blob([0])} // props
      ));
    });
    it("should return true if both propsImage and stateImage are equal normal images", () => {
      assert.isTrue(ScreenshotUtils.isRemoteImageLocal(
        {url: "test url"}, // state
        "test url" // props
      ));
    });
    it("should return false if both propsImage and stateImage are different normal images", () => {
      assert.isFalse(ScreenshotUtils.isRemoteImageLocal(
        {url: "test url 1"}, // state
        "test url 2" // props
      ));
    });
    it("should return false if both propsImage and stateImage are different type of images", () => {
      assert.isFalse(ScreenshotUtils.isRemoteImageLocal(
        {path: "/path1", url: "blob://test"}, // state
        "test url 2" // props
      ));
      assert.isFalse(ScreenshotUtils.isRemoteImageLocal(
        {url: "https://test-url"}, // state
        {path: "/path1", data: new Blob([0])} // props
      ));
    });
  });
  describe("#isBlob", () => {
    let state = {
      blobImage: {path: "/test", url: "blob://test"},
      normalImage: {url: "https://test-url"}
    };
    let props = {
      blobImage: {path: "/test", data: new Blob([0])},
      normalImage: "https://test-url"
    };
    it("should return false if image is null", () => {
      assert.isFalse(ScreenshotUtils.isBlob(true, null));
      assert.isFalse(ScreenshotUtils.isBlob(false, null));
    });
    it("should return true if image is a blob and type matches", () => {
      assert.isTrue(ScreenshotUtils.isBlob(true, state.blobImage));
      assert.isTrue(ScreenshotUtils.isBlob(false, props.blobImage));
    });
    it("should return false if image is not a blob and type matches", () => {
      assert.isFalse(ScreenshotUtils.isBlob(true, state.normalImage));
      assert.isFalse(ScreenshotUtils.isBlob(false, props.normalImage));
    });
    it("should return false if type does not match", () => {
      assert.isFalse(ScreenshotUtils.isBlob(false, state.blobImage));
      assert.isFalse(ScreenshotUtils.isBlob(false, state.normalImage));
      assert.isFalse(ScreenshotUtils.isBlob(true, props.blobImage));
      assert.isFalse(ScreenshotUtils.isBlob(true, props.normalImage));
    });
  });
});
