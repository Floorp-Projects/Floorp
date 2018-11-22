# WebRender
GPU renderer for the Web content, used by Servo.

## Update as a Dependency
After updating shaders in WebRender, go to servo and:

  * Go to the servo directory and do ./mach update-cargo -p webrender
  * Create a pull request to servo


## Use WebRender with Servo
To use a local copy of WebRender with servo, go to your servo build directory and:

  * Edit Cargo.toml
  * Add at the end of the file:

```
[patch."https://github.com/servo/webrender"]
"webrender" = { path = "<path>/webrender" }
"webrender_api" = { path = "<path>/webrender_api" }
```

where `<path>` is the path to your local copy of WebRender.

  * Build as normal

## Documentation

The Wiki has a [few pages](https://github.com/servo/webrender/wiki/) describing the internals and conventions of WebRender.

## Testing

Tests run using OSMesa to get consistent rendering across platforms.

Still there may be differences depending on font libraries on your system, for
example.

See [this gist](https://gist.github.com/finalfantasia/129cae811e02bf4551ac) for
how to make the text tests useful in Fedora, for example.
