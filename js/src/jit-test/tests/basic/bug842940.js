try { for (let v of wrapWithProto(new Proxy({}, {}), [])) { } } catch (e) {}
