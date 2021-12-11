var revocationFunction = Proxy.revocable({}, {}).revoke;
reportCompare(revocationFunction.name, "");
