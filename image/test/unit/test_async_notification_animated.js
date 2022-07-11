/*
 * Test for asynchronous image load/decode notifications in the case that the
 * image load works, but for an animated image.
 *
 * If this fails because a request wasn't cancelled, it's possible that
 * imgContainer::ExtractFrame didn't set the new image's status correctly.
 */

// transparent-animation.gif from the gif reftests.

var spec =
  "data:image/gif;base64,R0lGODlhZABkAIABAP8AAP///yH5BAkBAAEALAAAAABLAGQAAAK8jI+py+0Po5y02ouz3rz7D4biSJbmiabqyrbuC8fyTNf2jef6zvf+DwwKh8Si8YhMKpchgPMJjUqnVOipis1ir9qul+sNV8HistVkTj/JajG7/UXDy+95tm4fy/NdPF/q93dWIqgVWAhwWKgoyPjnyAeZJ2lHOWcJh9mmqcaZ5mkGSreHOCXqRloadRrGGkeoapoa6+TaN0tra4gbq3vHq+q7BVwqrMeEnKy8zNzs/AwdLT1NXW19jZ1tUgAAIfkECQEAAQAsAAAAADQAZAAAArCMj6nL7Q+jnLTai7PevPsPhuJIluaJpurKtu4Lx/JM1/aN5/rO9/7vAAiHxKLxiCRCkswmc+mMSqHSapJqzSof2u4Q67WCw1MuOTs+N9Pqq7kdZcON8vk2aF+/88g6358HaCc4Rwhn2IaopnjGSOYYBukl2UWpZYm2x0enuXnX4NnXGQqAKTYaalqlWoZH+snwWsQah+pJ64Sr5ypbCvQLHCw8TFxsfIycrLzM3PxQAAAh+QQJAQABACwAAAAAGwBkAAACUIyPqcvtD6OctNqLs968+w+G4kiW5omm6sq27gTE8kzX9o3n+s73/g8MCofEovGITCqXzKbzCY1Kp9Sq9YrNarfcrvdrfYnH5LL5jE6r16sCADs=";
var ioService = Services.io;
var uri = ioService.newURI(spec);

load("async_load_tests.js");
