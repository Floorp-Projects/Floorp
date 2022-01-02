// |jit-test| error:InternalError

"" + {toString: Date.prototype.toJSON};
