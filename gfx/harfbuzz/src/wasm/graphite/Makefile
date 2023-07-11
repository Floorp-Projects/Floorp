FONTS = CharisSIL-R.wasm.ttf Scheherazade-R.wasm.ttf AwamiNastaliq-Regular.wasm.ttf
ADD_TABLE = ../../addTable.py

all: $(FONTS)

%.wasm: %.cc ../../hb-wasm-api.h
	emcc \
		-I ../.. \
		-I ~/graphite/include/ \
		-fvisibility=hidden \
		-Wl,--allow-undefined \
		-Wl,--no-entry \
		-Wl,--strip-all \
		-sERROR_ON_UNDEFINED_SYMBOLS=0 \
		-Wl,--export=malloc -Wl,--export=free \
		~/graphite/src/libgraphite2.a \
		~/wasm/wasi-sdk-19.0/share/wasi-sysroot/lib/wasm32-wasi/libc.a \
		$< \
		-o $@


%.wasm.ttf: %.ttf shape.wasm $(ADD_TABLE)
	python $(ADD_TABLE) $< $@ shape.wasm

clean:
	$(RM) shape.wasm $(FONTS)

.PRECIOUS: shape.wasm
