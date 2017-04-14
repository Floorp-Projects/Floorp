var gl;
var video;

function onPlayingTestVideo() {
    video.removeEventListener("playing", onPlayingTestVideo, true);
    gl.texImage2D(gl.TEXTURE_2D, 0, gl.RGBA, gl.RGBA, gl.UNSIGNED_BYTE, video);
    is(gl.getError(), gl.NO_ERROR, "texImage2D should not generate any error here.");
    video.pause();
    SimpleTest.finish();
}

function startTest(file) {
    gl = document.createElement("canvas").getContext("webgl");
    ext = gl.getExtension("MOZ_debug");
    ok(ext, "MOZ_debug extenstion should exist");
    gl.bindTexture(gl.TEXTURE_2D, gl.createTexture());
    gl.pixelStorei(ext.UNPACK_REQUIRE_FASTPATH, true);
    is(gl.getError(), gl.NO_ERROR, "pixelStorei should not generate any error here.");

    video = document.createElement("video");
    video.addEventListener("playing", onPlayingTestVideo, true);
    video.preload = "auto";
    video.src = file;
    video.loop = true;
    video.play();
}
