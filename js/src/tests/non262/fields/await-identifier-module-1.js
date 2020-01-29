// |reftest| error:SyntaxError module

async () => class { [await] = 1 };
