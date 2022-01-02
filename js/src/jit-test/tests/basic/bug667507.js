for (i = 0; i < 10; i++) {
    Object.defineProperty({}, "", {
        get: function() {}
    })
    gc()
}
