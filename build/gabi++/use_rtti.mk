# To use RTTI, "include abi/cpp/use_rtti.mk" in your target.

LOCAL_C_INCLUDES := \
	abi/cpp/include \
	$(LOCAL_C_INCLUDES)

LOCAL_RTTI_FLAG := -frtti
LOCAL_SHARED_LIBRARIES += libgabi++
