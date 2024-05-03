// |jit-test| --enable-import-attributes; module; error: TypeError: Unsupported import attribute: unsupported
import a from 'foo' with { unsupported: 'test'}
