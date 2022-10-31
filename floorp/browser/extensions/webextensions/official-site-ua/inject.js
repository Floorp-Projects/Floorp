Object.defineProperty(window.screen, "availWidth", {
    get: function () { return window.innerWidth }
})

Object.defineProperty(window.screen, "width", {
    get: function () { return window.innerWidth }
})

Object.defineProperty(window.screen, "availHeight", {
    get: function () { return window.innerHeight }
})

Object.defineProperty(window.screen, "height", {
    get: function () { return window.innerHeight }
})
