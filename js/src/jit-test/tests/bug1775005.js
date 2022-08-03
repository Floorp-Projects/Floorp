try {
  var n = 65536;
  const lower = "a".repeat(n)
  const upper = "A".repeat(n)
  const pat = "^(?:" + lower + "|" + upper + "|" + upper + ")$";
  const re = RegExp(pat, "i");
  re.test("");
} catch {}
