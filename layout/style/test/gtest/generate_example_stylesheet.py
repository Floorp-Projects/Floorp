def main(output, stylesheet):
    css = open(stylesheet, "r").read()
    css = (
        css.replace("\\", "\\\\")
        .replace("\r", "\\r")
        .replace("\n", "\\n")
        .replace('"', '\\"')
    )

    # Work around "error C2026: string too big"
    # https://msdn.microsoft.com/en-us/library/dddywwsc.aspx
    chunk_size = 10000
    chunks = ('"%s"' % css[i : i + chunk_size] for i in range(0, len(css), chunk_size))

    header = "#define EXAMPLE_STYLESHEET " + " ".join(chunks)
    output.write(header)
