#ifndef frontend_ErrorReporter_h
#define frontend_ErrorReporter_h

namespace js {
namespace frontend {

class ErrorReporter
{
  public:
    virtual const ReadOnlyCompileOptions& options() const = 0;
    virtual void lineNumAndColumnIndex(size_t offset, uint32_t* line, uint32_t* column) const = 0;
    virtual size_t offset() const = 0;
    virtual bool hasTokenizationStarted() const = 0;
    virtual void reportErrorNoOffset(unsigned errorNumber, ...) = 0;
    virtual const char* getFilename() const = 0;
};

} // namespace frontend
} // namespace js

#endif // frontend_ErrorReporter_h
