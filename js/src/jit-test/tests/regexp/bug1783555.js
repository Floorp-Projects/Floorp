var src = "(.?)".repeat(65536);
try {
  "".match(src);
} catch {}
